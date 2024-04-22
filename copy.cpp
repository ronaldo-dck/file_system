#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <bitset>
#include <math.h>
#include <ctime>

// Estruturas

typedef struct BootRecord
{
    unsigned short int bytes_per_sector;
    char sectors_per_cluster;
    char reserved_sectors;
    unsigned long int total_sectors;
    unsigned long int bitmap_size;
    unsigned long int inodes;
    unsigned short int len_inode;
    unsigned long int pos_data;
} __attribute__((packed)) BootRecord;

typedef struct Inode
{
    char type;
    char aligment[3];
    unsigned long int size;
    unsigned long int modification_time;
    unsigned long int creation_time;
    unsigned short int cluster_ptrs[8];
    unsigned short int undirect_cluster_ptr;
} __attribute__((packed)) Inode;

// Constantes

std::string image_path;
std::string target_file_path;
std::fstream image_file;
std::fstream target_file;

BootRecord boot_record;

void get_inputs()
{
    std::cout << "== COPIAR ARQUVIO PARA SISTEMA ROTOM FS ==\n";
    std::cout << "Selecionar caminho (path) da imagem Rotom FS: ";
    std::cin >> image_path;
    std::cout << "Selecionar caminho (path) do arquivo a ser copiado: ";
    std::cin >> target_file_path;
}

void print_br()
{
    std::cout << "BytesPerSector: " << (int)boot_record.bytes_per_sector << "\n";
    std::cout << "SectorsPerCluster: " << (int)boot_record.sectors_per_cluster << "\n";
    std::cout << "ReservedSectors: " << (int)boot_record.reserved_sectors << "\n";
    std::cout << "TotalOfSectors: " << (int)boot_record.total_sectors << "\n";
    std::cout << "SizeOfBitMap: " << (int)boot_record.bitmap_size << "\n";
    std::cout << "Inodes: " << (int)boot_record.inodes << "\n";
    std::cout << "LenOfInode: " << (int)boot_record.len_inode << "\n";
    std::cout << "PosData: " << (int)boot_record.pos_data << "\n";
}

std::vector<unsigned short> find_clusters(int clusters_needed)
{
    std::vector<unsigned short> free_clusters;
    int bitmap_pos = boot_record.reserved_sectors * boot_record.bytes_per_sector;
    image_file.seekg(bitmap_pos, std::ios::beg);
    for (int i = 0; i < boot_record.bitmap_size && free_clusters.size() < clusters_needed; i++)
    {
        char byte;
        image_file.read(&byte, 1);
        for (int j = 0; j < 8; j++)
        {
            if (!(byte & (1 << j)))
                free_clusters.push_back(i * 8 + j);
        }
    }
    if (free_clusters.size() < clusters_needed)
    {
        std::cout << "Não há clusters disponíveis o suficiente para armazenar o arquivo.\n";
        std::cout << "Terminando...\n";
        target_file.close();
        image_file.close();
        std::exit(1);
    }
    return free_clusters;
}

void allocate_file(std::vector<unsigned short> free_clusters)
{
    int bitmap_pos = boot_record.reserved_sectors * boot_record.bytes_per_sector;
    int inodes_pos = bitmap_pos + boot_record.bitmap_size * boot_record.bytes_per_sector;
    image_file.seekg(inodes_pos, std::ios::beg);
    int available_inode = 0;
    Inode inode;
    for (int i = 0; i < boot_record.inodes && !available_inode; i++)
    {
        image_file.read((char *)&inode, sizeof(Inode));
        if (inode.type == 0x00)
            available_inode = i;
    }
    // Último Check (Acredito eu)
    if (!available_inode)
    {
        std::cout << "Não há inodes disponíveis.\n";
        std::cout << "Terminando...\n";
        image_file.close();
        target_file.close();
        std::exit(1);
    }
    // Alocar Inode
    target_file.seekg(0, std::ios::end);
    
    inode.type = 0x01;
    inode.size = target_file.tellg();;
    inode.modification_time = static_cast<unsigned int>(time(0));
    inode.creation_time = static_cast<unsigned int>(time(0));
    // Alocar Clusters no Bitmap
    for (int i = 0; i < free_clusters.size(); i++)
    {
        int byte_loc = free_clusters[i] / 8; // Bit 9 = Byte 1 Bit 1
        int bit = free_clusters[i] - byte_loc * 8; // 9 - 1 * 8 = 1
        image_file.seekg(bitmap_pos + byte_loc, std::ios::beg);
        image_file.seekp(bitmap_pos + byte_loc, std::ios::beg);
        char read_byte;
        image_file.read(&read_byte, 1);
        char new_byte = read_byte | (1 << bit);
        image_file.write(&new_byte, 1);
    }
    // Alocar Ponteiros Diretos
    for (int i = 0; i < 8 && i < free_clusters.size(); i++)
        inode.cluster_ptrs[i] = free_clusters[i];
    // Alocar Ponteiros Indiretos
    if (free_clusters.size() > 8)
    {   
        inode.undirect_cluster_ptr = free_clusters[8];
        int pointers_per_cluster = (float)(boot_record.sectors_per_cluster * boot_record.bytes_per_sector) / 4 - 1;
        int clusters_allocated = 8;
        int data_pos = inodes_pos + boot_record.inodes;
        while (clusters_allocated != free_clusters.size())
        {
            clusters_allocated++;
            image_file.seekp(data_pos + free_clusters[clusters_allocated] * boot_record.sectors_per_cluster * boot_record.bytes_per_sector, std::ios::beg);
            for (int i = 0; i < pointers_per_cluster && clusters_allocated != free_clusters.size(); i++)
            {
                image_file.write(reinterpret_cast<const char*>(free_clusters[clusters_allocated]), 4);    
                clusters_allocated++;
            }
            if (clusters_allocated != free_clusters.size())
                image_file.write(reinterpret_cast<const char*>(free_clusters[clusters_allocated]), 4);
        }
    }
    image_file.seekp(inodes_pos + available_inode * sizeof(Inode), std::ios::beg);
    image_file.write((char *)&inode, sizeof(Inode));
}

int main(int argc, char *argv[])
{
    // Entradas
    get_inputs();
    // Conseguir Boot Record
    image_file.open(image_path, std::ios::binary | std::ios::in | std::ios::out);
    image_file.read((char *)(&boot_record), sizeof(BootRecord));
    print_br();
    // Ler Arquivo Alvo
    target_file.open(target_file_path, std::ios::binary | std::ios::in | std::ios::out);
    target_file.seekg(0, std::ios::end);
    int target_file_size = target_file.tellg();
    std::cout << "Tamanho do arquivo a ser copiado: " << target_file_size << " bytes\n";
    // Cálculo Clusters Necessários
    float sectors_needed = (float)target_file_size / (float)boot_record.bytes_per_sector;
    int clusters_needed = std::ceil((float)sectors_needed / (float)boot_record.sectors_per_cluster);
    int pointers_per_cluster = (float)(boot_record.sectors_per_cluster * boot_record.bytes_per_sector) / 4 - 1;
    clusters_needed += std::ceil((float)(clusters_needed - 8) / (float)pointers_per_cluster);
    std::cout << "Clusters necessários para armazenar o arquivo: " << clusters_needed << "\n";
    // Verificar Disponibilidade de Clusters e Guardar Disponíveis
    std::vector<unsigned short> free_clusters = find_clusters(clusters_needed);
    // Procurar e Alocar Inode Disponível e Transferir Dados
    allocate_file(free_clusters);
    // Fechar Arquivo e Imagem
    target_file.close();
    image_file.close();
    return 0;
}
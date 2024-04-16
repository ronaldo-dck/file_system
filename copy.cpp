#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <bitset>
#include <math.h>

// Estruturas

typedef struct BootRecord
{
    unsigned short int bytes_per_sector;
    char sectors_per_cluster;
    unsigned short int reserved_sectors;
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
    unsigned short int cluster_ptr1;
    unsigned short int cluster_ptr2;
    unsigned short int cluster_ptr3;
    unsigned short int cluster_ptr4;
    unsigned short int cluster_ptr5;
    unsigned short int cluster_ptr6;
    unsigned short int cluster_ptr7;
    unsigned short int cluster_ptr8;
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

std::vector<int> find_clusters(int clusters_needed)
{
    std::vector<int> free_clusters;
    int bitmap_pos = boot_record.reserved_sectors * boot_record.bytes_per_sector;
    image_file.seekg(bitmap_pos, std::ios::beg);
    for (int i = 0; i < boot_record.bitmap_size && free_clusters.size() >= clusters_needed; i++)
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

void allocate_file(std::vector<int> free_clusters)
{
    int inodes_pos = boot_record.reserved_sectors * boot_record.bytes_per_sector + boot_record.bitmap_size;
    image_file.seekg(inodes_pos, std::ios::beg);
    int available_inode = 0;
    Inode inode;
    for (int i = 0; i < boot_record.inodes && !available_inode; i++)
    {
        image_file.read((char *)&inode, sizeof(Inode));
        if (inode.type == 0x00)
            available_inode = i;
    }
    if (!available_inode)
    {
        std::cout << "Não há inodes disponíveis.\n";
        std::cout << "Terminando...\n";
        image_file.close();
        target_file.close();
        std::exit(1);
    }
    // Alocar Inode
    inode.type = 0x01;
    
    image_file.seekp(inodes_pos + available_inode * sizeof(Inode), std::ios::beg);
    image_file.write((char *)&inode, sizeof(Inode));
    for (int i = 0; i < free_clusters.size(); i++)
    {

    }
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
    std::cout << "Clusters necessários para armazenar o arquivo: " << clusters_needed << "\n";
    // Verificar Disponibilidade de Clusters e Guardar Disponíveis
    std::vector<int> free_clusters = find_clusters(clusters_needed);
    // Procurar e Alocar Inode Disponível e Transferir Dados
    allocate_file(free_clusters);
    // Fechar Arquivo e Imagem
    target_file.close();
    image_file.close();
    return 0;
}
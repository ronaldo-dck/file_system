#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <bitset>
#include <math.h>
#include <ctime>
#include <string>
#include <stdio.h>

// Estruturas

typedef struct BootRecord
{
    unsigned short int bytes_per_sector;
    char sectors_per_cluster;
    char reserved_sectors;
    unsigned long int total_sectors;
    unsigned long int bitmap_size;
    unsigned long int inodes;
    unsigned int len_inode;
    unsigned long int pos_data;
} __attribute__((packed)) BootRecord;

typedef struct Inode
{
    char type;
    char aligment[3];
    unsigned long int size;
    unsigned long int modification_time;
    unsigned long int creation_time;
    unsigned int cluster_ptrs[8];
    unsigned int undirect_cluster_ptr;
} __attribute__((packed)) Inode;

typedef struct RootEntry
{
    char type;
    unsigned long int inode_id;
    char name[20];
    char extension[3];
} __attribute__((packed)) RootEntry;

// Globais

std::string image_path;
std::string target_file_path;
std::fstream image_file;
std::fstream target_file;

BootRecord boot_record;

bool expand_root_dir = false;
unsigned int root_dir_new_cluster = 0x00;

int bitmap_position, inodes_position, data_position;
int target_file_size, pointers_per_cluster;

Inode root_dir_inode;

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

std::vector<unsigned int> find_clusters(int clusters_needed)
{
    std::vector<unsigned int> free_clusters;
    image_file.seekg(bitmap_position, std::ios::beg);
    int bitmap_bytes = boot_record.bitmap_size * boot_record.bytes_per_sector;
    int bitmap_offset;
    for (bitmap_offset = 0; bitmap_offset < bitmap_bytes && free_clusters.size() < clusters_needed; bitmap_offset++)
    {
        char byte;
        image_file.read(&byte, 1);
        for (int j = 0; j < 8; j++)
        {
            if (!(byte & (128 >> (uint)j)))
            {
                free_clusters.push_back(bitmap_offset * 8 + j + 1);
                if (free_clusters.size() == clusters_needed)
                    break;
            }
        }
    }
    // Verificar Falta
    if (free_clusters.size() < clusters_needed)
    {
        std::cout << "Não há clusters disponíveis o suficiente para armazenar o arquivo.\n";
        std::cout << "Terminando...\n";
        target_file.close();
        image_file.close();
        std::exit(1);
    }
    // Verificar Root Dir
    image_file.seekg(bitmap_position + boot_record.bitmap_size * boot_record.bytes_per_sector, std::ios::beg);
    image_file.read((char *)&root_dir_inode, sizeof(Inode));
    int new_root_dir_size = root_dir_inode.size + 32;
    int root_dir_clusters_needed = std::ceil((float)root_dir_inode.size / ((float)boot_record.bytes_per_sector * boot_record.sectors_per_cluster));
    root_dir_clusters_needed = std::max(1, root_dir_clusters_needed);
    int new_root_dir_clusters_needed = std::ceil((float)new_root_dir_size / ((float)boot_record.bytes_per_sector * boot_record.sectors_per_cluster));
    bool found_root_cluster = false;
    image_file.seekg(bitmap_position + bitmap_offset, std::ios::beg);
    // Encontrar Cluster para Root Dir Se Necessário
    if (new_root_dir_clusters_needed > root_dir_clusters_needed)
    {
        expand_root_dir = true;
        for (int k = bitmap_offset + 1; k < bitmap_bytes - bitmap_offset && !found_root_cluster; k++)
        {
            char byte;
            image_file.read(&byte, 1);
            for (int j = 0; j < 8 && !found_root_cluster; j++)
            {
                if (!(byte & (128 >> (uint)j)))
                {
                    root_dir_new_cluster = k * 8 + j + 1;
                    found_root_cluster = true;
                    printf("Expand root dir to %i\n", root_dir_new_cluster);
                }
            }
        }
        if (!root_dir_new_cluster)
        {
            printf("Não há clusters disponíveis para expandir o root dir.\n");
            printf("Saindo...\n");
            std::exit(1);
        }
    }
    return free_clusters;
}

int verify_inode_availability()
{
    image_file.seekg(inodes_position, std::ios::beg);
    Inode temp_inode;
    for (int i = 0; i < boot_record.inodes; i++)
    {
        image_file.read((char *)&temp_inode, sizeof(Inode));
        if (temp_inode.type == 0x00)
            return i;
    }
    return -1;
}

RootEntry create_root_entry(int allocated_inode)
{
    RootEntry root_entry;
    root_entry.type = 0x10;
    root_entry.inode_id = allocated_inode;
    int name_loc;
    for (int i = 0; i < 20; i++)
        root_entry.name[i] = 0x00;
    for (int i = 0; i < 3; i++)
        root_entry.extension[i] = 0x00;
    for (int i = target_file_path.length() - 1; i >= -1; i--)
    {
        if (i == -1 || target_file_path[i] == '/')
            name_loc = i;
    }
    for (int j = name_loc + 1; j < target_file_path.length() && target_file_path[j] != '.'; j++)
        root_entry.name[j - (name_loc) - 1] = target_file_path[j];
    for (int j = name_loc + 1; j < target_file_path.length(); j++)
    {
        if (target_file_path[j] == '.')
        {
            for (int k = j + 1; k < target_file_path.length(); k++)
                root_entry.extension[k - j - 1] = target_file_path[k];
        }
    }
    return root_entry;
}

void allocate_root_entry(int allocated_inode)
{
    image_file.seekg(inodes_position, std::ios::beg);
    Inode new_root_dir = root_dir_inode;
    new_root_dir.size += 32;
    RootEntry root_entry = create_root_entry(allocated_inode);
    int root_dir_clusters = std::ceil((float)new_root_dir.size / ((float)boot_record.bytes_per_sector * boot_record.sectors_per_cluster));
    printf("New root size: %i\n", new_root_dir.size);
    printf("Root dir clusters: %i\n", root_dir_clusters);
    if (expand_root_dir)
    {
        int entry_loc = boot_record.pos_data * boot_record.bytes_per_sector + (root_dir_new_cluster - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
        image_file.seekp(entry_loc, std::ios::beg);
        image_file.write((char *)&root_entry, sizeof(RootEntry));
        if (root_dir_clusters < 9)
            new_root_dir.cluster_ptrs[root_dir_clusters - 1] = root_dir_new_cluster;
        else
        {
            printf("The undirect expand\n");
            int current_indirect_cluster = new_root_dir.undirect_cluster_ptr;
            int current_cluster = 7;
            int ind_cluster_loc = boot_record.pos_data * boot_record.bytes_per_sector + (current_indirect_cluster - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
            image_file.seekg(ind_cluster_loc, std::ios::beg);
            while (current_cluster != root_dir_clusters - 1)
            {
                current_cluster++;
                if ((current_cluster - 8) % pointers_per_cluster == 0)
                {
                    image_file.seekg(ind_cluster_loc + pointers_per_cluster * 4, std::ios::beg);
                    image_file.read((char *)&current_indirect_cluster, 4);
                    ind_cluster_loc = boot_record.pos_data * boot_record.bytes_per_sector + (current_indirect_cluster - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
                }
            }
            int ind_count = (current_cluster - 8) / pointers_per_cluster;
            int ind_offset = (current_cluster - 8) - ind_count * pointers_per_cluster;
            image_file.seekg(ind_cluster_loc + ind_offset * 4, std::ios::beg);
            image_file.seekp(image_file.tellg(), std::ios::beg);
            image_file.write((char *)&root_dir_new_cluster, 4);
        }
    }
    else
    {
        int entries_per_cluster = (boot_record.bytes_per_sector * boot_record.sectors_per_cluster) / sizeof(RootEntry);
        if (root_dir_clusters < 8)
        {
            printf("The direct non expand\n");
            bool allocated = false;
            for (int i = 0; i < 8 && !allocated; i++)
            {
                int cluster = new_root_dir.cluster_ptrs[i];
                RootEntry current_entry;
                image_file.seekg(boot_record.pos_data * boot_record.bytes_per_sector + (cluster - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster, std::ios::beg);
                for (int j = 0; j < entries_per_cluster && !allocated; j++)
                {
                    image_file.read((char *)&current_entry, sizeof(RootEntry));
                    if (current_entry.type == 0x00)
                    {
                        int entry_loc = (int)image_file.tellg() - sizeof(RootEntry);
                        image_file.seekp(entry_loc, std::ios::beg);
                        image_file.write(reinterpret_cast<char *>(&root_entry), sizeof(RootEntry));
                        allocated = true;
                    }
                }
            }
        }
        else
        {
            printf("The undirect non expand\n");
            int current_cluster = 0;
            int cluster;
            bool allocated = false;
            int current_indirect_cluster = new_root_dir.undirect_cluster_ptr;
            while (current_cluster < root_dir_clusters && !allocated)
            {
                if (current_cluster < 8)
                {
                    for (int i = 0; i < 8 && current_cluster < root_dir_clusters && !allocated; i++)
                    {
                        current_cluster = i;
                        cluster = new_root_dir.cluster_ptrs[i];
                        int cluster_loc = boot_record.pos_data * boot_record.bytes_per_sector + (cluster - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
                        image_file.seekg(cluster_loc, std::ios::beg);
                        RootEntry current_entry;
                        for (int j = 0; j < entries_per_cluster && !allocated; j++)
                        {
                            image_file.read((char *)&current_entry, sizeof(RootEntry));
                            if (current_entry.type == 0x00)
                            {
                                printf("Wrote at cluster: %i Entry: %i\n", cluster, j);
                                allocated = true;
                                image_file.seekp((int)image_file.tellg() - sizeof(RootEntry), std::ios::beg);
                                image_file.write((char *)&root_entry, sizeof(RootEntry));
                            }
                        }
                    }
                }
                else
                {
                    int ind_cluster_loc = boot_record.pos_data * boot_record.bytes_per_sector + (current_indirect_cluster - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
                    image_file.seekg(ind_cluster_loc, std::ios::beg);
                    for (int i = 0; i < pointers_per_cluster && current_cluster < root_dir_clusters && !allocated; i++)
                    {
                        current_cluster++;
                        image_file.read((char *)&cluster, 4);
                        image_file.seekg(data_position + (cluster - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster, std::ios::beg);
                        RootEntry current_entry;
                        for (int j = 0; j < entries_per_cluster && !allocated; j++)
                        {
                            image_file.read((char *)&current_entry, sizeof(RootEntry));
                            if (current_entry.type == 0x00)
                            {
                                printf("Wrote at cluster: %i Entry: %i\n", cluster, j);
                                allocated = true;
                                image_file.seekp((int)image_file.tellg() - sizeof(RootEntry), std::ios::beg);
                                image_file.write((char *)&root_entry, sizeof(RootEntry));
                            }
                        }
                    }
                    image_file.read((char *)&current_indirect_cluster, 4);
                }
            }
        }
    }
    image_file.seekp(inodes_position, std::ios::beg);
    image_file.write((char *)&new_root_dir, sizeof(Inode));
}

std::vector<unsigned int> allocate_file(std::vector<unsigned int> free_clusters)
{
    std::vector<unsigned int> data_clusters;
    int allocated_inode = verify_inode_availability();
    // Último Check (Acredito eu)
    if (allocated_inode == -1)
    {
        std::cout << "Não há inodes disponíveis.\n";
        std::cout << "Terminando...\n";
        image_file.close();
        target_file.close();
        std::exit(1);
    }
    // Lidar com Root Dir
    allocate_root_entry(allocated_inode);
    // Criar Inode
    Inode inode;
    target_file.seekg(0, std::ios::end);
    inode.type = 0x01;
    inode.size = target_file.tellg();
    inode.modification_time = static_cast<unsigned int>(time(0));
    inode.creation_time = static_cast<unsigned int>(time(0));
    // Alocar Clusters no Bitmap
    for (int i = 0; i < free_clusters.size(); i++)
    {
        int byte_loc = (free_clusters[i] - 1) / 8; // Bit 9 = Byte 1 Bit 1
        uint bit = (free_clusters[i] - 1) % 8;     // 9 - 1 * 8 = 1
        image_file.seekg(bitmap_position + byte_loc, std::ios::beg);
        char read_byte;
        image_file.read(&read_byte, sizeof(read_byte));
        char new_byte = read_byte | (128 >> (uint)bit);
        image_file.seekp((int)image_file.tellg() - 1, std::ios::beg); // C/C++ Não faz sentido
        image_file.write(&new_byte, sizeof(new_byte));
    }
    if (expand_root_dir)
    {
        int root_dir_byte_loc = (root_dir_new_cluster - 1) / 8;
        uint root_dir_bit = (root_dir_new_cluster - 1) % 8;
        image_file.seekg(bitmap_position + root_dir_byte_loc, std::ios::beg);
        char read_byte;
        image_file.read(&read_byte, sizeof(read_byte));
        char new_byte = read_byte | (128 >> (uint)root_dir_bit);
        image_file.seekp((int)image_file.tellg() - 1, std::ios::beg);
        image_file.write(&new_byte, sizeof(new_byte));
    }
    // Alocar Ponteiros Diretos
    for (int i = 0; i < 8 && i < free_clusters.size(); i++)
    {
        inode.cluster_ptrs[i] = free_clusters[i];
        data_clusters.push_back(free_clusters[i]);
    }
    // Alocar Ponteiros Indiretos
    if (free_clusters.size() > 8)
    {
        inode.undirect_cluster_ptr = free_clusters[8];
        int clusters_allocated = 8;
        int data_pos = boot_record.pos_data * boot_record.bytes_per_sector;
        while (clusters_allocated < free_clusters.size())
        {
            int ind_loc = data_pos + (free_clusters[clusters_allocated] - 1) * boot_record.sectors_per_cluster * boot_record.bytes_per_sector;
            image_file.seekp(ind_loc, std::ios::beg);
            clusters_allocated++;
            for (int i = 0; i < pointers_per_cluster && clusters_allocated < free_clusters.size(); i++)
            {
                image_file.write(reinterpret_cast<const char *>(&free_clusters[clusters_allocated]), sizeof(unsigned short));
                data_clusters.push_back(free_clusters[clusters_allocated]);
                clusters_allocated++;
            }
            if (clusters_allocated < free_clusters.size())
            {
                clusters_allocated++;
                image_file.write(reinterpret_cast<const char *>(&free_clusters[clusters_allocated]), sizeof(unsigned short));
            }
        }
    }
    // Write Inode
    image_file.seekp(inodes_position + allocated_inode * sizeof(Inode), std::ios::beg);
    image_file.write((char *)&inode, sizeof(Inode));
    return data_clusters;
}

void copy_data(std::vector<unsigned int> data_clusters)
{
    printf("Data clusters used: [");
    for (int data_cluster : data_clusters)
    {
        printf("%i, ", data_cluster);
    }
    printf("]\n");
    target_file.seekg(0, std::ios::end);
    int file_size = target_file.tellg();
    target_file.seekg(0, std::ios::beg);
    for (int i = 0; i < data_clusters.size(); i++)
    {
        int data_cluster_loc = boot_record.pos_data * boot_record.bytes_per_sector + (data_clusters[i] - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
        image_file.seekp(data_cluster_loc, std::ios::beg);
        char *read_data = new char[boot_record.bytes_per_sector * boot_record.sectors_per_cluster];
        if (i != data_clusters.size() - 1)
        {
            target_file.read(read_data, boot_record.bytes_per_sector * boot_record.sectors_per_cluster);
            image_file.write(read_data, boot_record.bytes_per_sector * boot_record.sectors_per_cluster);
        }
        else
        {
            delete read_data;
            read_data = new char[file_size - (int)target_file.tellg()];
            int data_left = file_size - (int)target_file.tellg();
            target_file.read(read_data, data_left);
            image_file.write(read_data, data_left);
        }
    }
}

int main(int argc, char *argv[])
{
    // Entradas
    image_path = argv[1];
    target_file_path = argv[2];
    // Conseguir Boot Record
    image_file.open(image_path, std::ios::binary | std::ios::in | std::ios::out);
    image_file.read((char *)(&boot_record), sizeof(BootRecord));
    print_br();
    bitmap_position = boot_record.reserved_sectors * boot_record.bytes_per_sector;
    inodes_position = bitmap_position + boot_record.bitmap_size * boot_record.bytes_per_sector;
    data_position = inodes_position + boot_record.inodes * sizeof(Inode);
    // Ler Arquivo Alvo
    target_file.open(target_file_path, std::ios::binary | std::ios::in | std::ios::out);
    target_file.seekg(0, std::ios::end);
    target_file_size = target_file.tellg();
    std::cout << "Tamanho do arquivo a ser copiado: " << target_file_size << " bytes\n";
    // Cálculo Clusters Necessários
    float sectors_needed = (float)target_file_size / (float)boot_record.bytes_per_sector;
    int clusters_needed = std::ceil((float)sectors_needed / (float)boot_record.sectors_per_cluster);
    pointers_per_cluster = (float)(boot_record.sectors_per_cluster * boot_record.bytes_per_sector) / 4 - 1;
    clusters_needed += std::ceil((float)(clusters_needed - 8) / (float)pointers_per_cluster);
    std::cout << "Clusters necessários para armazenar o arquivo: " << clusters_needed << "\n";
    // Adquirir Root Dir
    image_file.seekg(inodes_position, std::ios::beg);
    image_file.read((char *)(&root_dir_inode), sizeof(Inode));
    // Verificar Disponibilidade de Clusters e Guardar Disponíveis
    std::vector<unsigned int> free_clusters = find_clusters(clusters_needed);
    // Procurar e Alocar Inode Disponível e Transferir Dados
    std::vector<unsigned int> data_clusters = allocate_file(free_clusters);
    // Escrever os dados
    copy_data(data_clusters);
    // Fechar Arquivo e Imagem
    target_file.close();
    image_file.close();
    return 0;
}
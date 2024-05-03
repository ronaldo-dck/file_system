#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <bitset>
#include <math.h>
#include <ctime>
#include <string>
#include <stdio.h>
#include <list>

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
    unsigned int indirect_cluster_ptr;
} __attribute__((packed)) Inode;

typedef struct RootEntry
{
    char type;
    unsigned long int inode_id;
    char name[20];
    char extension[3];
} __attribute__((packed)) RootEntry;

// Estruturas

std::vector<int> get_clusters(Inode node);

// Globais

std::string image_path;
std::string target_file_path;
std::fstream image_file;
std::fstream target_file;

BootRecord boot_record;

unsigned int root_dir_new_cluster = 0x00, root_dir_new_indirect_cluster = 0x00;

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

void print_rootEntry(RootEntry entry)
{
    std::cout << "Type: " << (int)entry.type << "\n";
    std::cout << "Name: " << entry.name << "\n";
}

void print_inode(Inode inode)
{
    std::cout << "Type: " << (int)inode.type << "\n";
    std::cout << "Size: " << (int)inode.size << "\n";
    std::cout << "ModificationTime: " << inode.modification_time << "\n";
    std::cout << "CreationTime: " << inode.creation_time << "\n";
}

void read_data(std::vector<int> data_clusters, int file_size, std::string destiny_path, bool write_file)
{
    std::ofstream save_file(destiny_path, std::ios::binary);
    char *read_data = new char[boot_record.bytes_per_sector * boot_record.sectors_per_cluster];
    for (int i = 0; i < data_clusters.size() - 1; i++)
    {
        int data_cluster_loc = data_position + (data_clusters[i] - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
        image_file.seekg(data_cluster_loc, std::ios::beg);
        image_file.read(read_data, boot_record.bytes_per_sector * boot_record.sectors_per_cluster);
        for (int j = 0; j < boot_record.bytes_per_sector * boot_record.sectors_per_cluster; j++)
            save_file << read_data[j];
    }
    delete read_data;
    image_file.seekg(data_position + (data_clusters[data_clusters.size() - 1] - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster, std::ios::beg);
    int data_left = file_size - (data_clusters.size() - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
    read_data = new char[data_left];
    image_file.read(read_data, data_left);
    for (int j = 0; j < data_left; j++)
        save_file << read_data[j];
    delete read_data;
    save_file.close();
}

std::vector<int> get_clusters(Inode node)
{
    std::vector<int> clusters;
    // pegar os ponteiros diretos
    for (int i = 0; i < 8; i++)
    {
        if (node.cluster_ptrs[i] == 0x00)
            break;
        clusters.push_back(node.cluster_ptrs[i]);
    }

    int indirect_ptr = node.indirect_cluster_ptr;


    while (indirect_ptr != 0x00)
    {
        image_file.seekg(data_position + (indirect_ptr - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster, std::ios::beg);

        int pointer = 0, count = 1;

        image_file.read(reinterpret_cast<char *>(&pointer), sizeof(int));
        while (pointer != 0x00 && count < pointers_per_cluster)
        {
            count++;
            clusters.push_back(pointer);
            image_file.read(reinterpret_cast<char *>(&pointer), sizeof(int));
        }

        indirect_ptr = 0;
        if (count == pointers_per_cluster)
            image_file.read(reinterpret_cast<char *>(&indirect_ptr), sizeof(int));
    }

    return clusters;
}

std::vector<RootEntry> get_root_entries()
{
    std::vector<RootEntry> entries;
    std::vector<int> root_dir_clusters = get_clusters(root_dir_inode);

    for (int i = 0; i < root_dir_clusters.size(); i++)
    {
        int cluster_loc = data_position + (root_dir_clusters[i] - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
        image_file.seekg(cluster_loc, std::ios::beg);
        RootEntry entry;
        int entries_per_cluster = (boot_record.sectors_per_cluster * boot_record.bytes_per_sector) / sizeof(RootEntry);
        for (int j = 0; j < entries_per_cluster; j++)
        {
            image_file.read((char *)(&entry), sizeof(RootEntry));
            if (entry.type == 0x10)
                entries.push_back(entry);
        }
    }
    printf("Root entries size: %i\n", entries.size());
    return entries;
}

int main(int argc, char *argv[])
{
    // Entradas
    image_path = argv[1];
    // Conseguir Boot Record
    image_file.open(image_path, std::ios::binary | std::ios::in | std::ios::out);
    image_file.read((char *)(&boot_record), sizeof(BootRecord));
    printf("=== Informações da Imagem ===\n\n");
    print_br();
    printf("\n");
    bitmap_position = boot_record.reserved_sectors * boot_record.bytes_per_sector;
    inodes_position = bitmap_position + boot_record.bitmap_size * boot_record.bytes_per_sector;
    pointers_per_cluster = (float)(boot_record.sectors_per_cluster * boot_record.bytes_per_sector) / 4 - 1;
    data_position = inodes_position + boot_record.inodes * sizeof(Inode);
    // Adquirir Root Dir
    image_file.seekg(inodes_position, std::ios::beg);
    image_file.read((char *)(&root_dir_inode), sizeof(Inode));

    std::vector<RootEntry> root_entries = get_root_entries();

    while (true)
    {
        // Listar Entradas do Diretório Raíz

        printf("=== Diretório Raiz ===\n");
        for (int i = 0; i < root_entries.size(); i++)
        {
            printf("%d --- ", i + 1);
            for (int j = 0; j < 20 && root_entries[i].name[j] != 0x00; j++)
                printf("%c", root_entries[i].name[j]);
            printf(".");
            for (int j = 0; j < 3 && root_entries[i].extension != 0x00; j++)
                printf("%c", root_entries[i].extension[j]);
            printf("\n");
        }
        printf("\n");
        printf("Digite o número do arquivo a ser digitado (ou '0' para sair): ");
        int input;
        std::cin >> input;
        if (input == 0)
            break;
        else
        {
            for (int i = 0; i < root_entries.size(); i++)
            {
                if (input - 1 == i)
                {
                    image_file.seekg(inodes_position + root_entries[i].inode_id * sizeof(Inode), std::ios::beg);
                    Inode inode;
                    image_file.read((char *)&inode, sizeof(Inode));
                    std::vector<int> clusters = get_clusters(inode);
                    printf("Deseja exportar o arquivo? Digite 1 para sim ou 0 para não\n");
                    std::cin >> input;
                    std::string file_name;
                    std::vector<char> file_vector;
                    for (int j = 0; j < 20 && root_entries[i].name[j] != 0x00; j++)
                        file_vector.push_back(root_entries[i].name[j]);
                    file_vector.push_back('.');
                    for (int j = 0; j < 3 && root_entries[i].extension[j] != 0x00; j++)
                        file_vector.push_back(root_entries[i].extension[j]);
                    file_name = std::string(file_vector.data(), file_vector.size());                    
                    if (input)
                    {
                        read_data(clusters, inode.size, file_name, true);
                        printf("Arquivo exportado.\n");
                    }
                    else
                        read_data(clusters, inode.size, file_name, false);
                    break;
                }
            }
        }
    }
    // Fechar Arquivo e Imagem
    target_file.close();
    image_file.close();
    return 0;
}
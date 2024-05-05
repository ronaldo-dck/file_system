#ifndef COPY_HH
#define COPY_HH

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <bitset>
#include <math.h>
#include <ctime>
#include <string>
#include <stdio.h>
#include "rfstruct.hh"

std::vector<unsigned int> find_clusters(Boot_Record boot_record, std::fstream &image_stream, int clusters_needed, bool &success)
{
    // Posições
    int bitmap_position = boot_record.reserved_sectors * boot_record.bytes_per_sector;
    // Encontra Clusters para os Dados e Root Dir (Se necessário)
    std::vector<unsigned int> free_clusters;
    image_stream.seekg(bitmap_position, std::ios::beg);
    int bitmap_bytes = boot_record.bitmap_size * boot_record.bytes_per_sector;
    int bitmap_offset = 0, bit_offset = 0;
    for (; bitmap_offset < bitmap_bytes && free_clusters.size() < clusters_needed; bitmap_offset++)
    {
        char byte;
        image_stream.read(&byte, 1);
        for (; bit_offset < 8; bit_offset++)
        {
            if (!(byte & (128 >> (uint)bit_offset)))
            {
                unsigned int free_cluster = bitmap_offset * 8 + bit_offset + 1;
                free_clusters.push_back(free_cluster);
                if (free_clusters.size() == clusters_needed)
                {
                    bit_offset++;
                    break;
                }
            }
        }
        if (free_clusters.size() < clusters_needed)
            bit_offset = 0;
    }
    // Verificar Falta
    if (free_clusters.size() < clusters_needed)
        success = false;
    else
        success = true;
    return free_clusters;
}

int get_available_inode_index(Boot_Record boot_record, std::fstream &image_stream)
{
    // Posições
    int inodes_position = boot_record.reserved_sectors * boot_record.bytes_per_sector + boot_record.bitmap_size * boot_record.bytes_per_sector;
    // Verificação de Inodes disponíveis
    image_stream.seekg(inodes_position, std::ios::beg);
    Inode temp_inode;
    for (int i = 0; i < boot_record.inodes; i++)
    {
        image_stream.read((char *)&temp_inode, sizeof(Inode));
        if (temp_inode.type == 0x00)
            return i;
    }
    return -1;
}

Root_Entry create_root_entry(Boot_Record boot_record, std::fstream &image_stream, std::string file_name)
{
    Root_Entry root_entry;
    root_entry.type = 0x10;
    root_entry.inode_id = get_available_inode_index(boot_record, image_stream);
    int name_loc;
    for (int i = 0; i < 20; i++)
        root_entry.name[i] = 0x00;
    for (int i = 0; i < 3; i++)
        root_entry.extension[i] = 0x00;
    for (int i = file_name.length() - 1; i >= -1; i--)
    {
        if (i == -1 || file_name[i] == '/')
            name_loc = i;
    }
    for (int j = name_loc + 1; j < file_name.length() && file_name[j] != '.' && j <= 20; j++)
        root_entry.name[j - (name_loc)-1] = file_name[j];

    if (root_entry.name[0] == 0x00)
    {
        for (int j = name_loc + 1; j < file_name.length() && j <= 20; j++)
            root_entry.name[j - (name_loc)-1] = file_name[j];
    }
    else
        for (int j = name_loc + 1; j < file_name.length(); j++)
        {
            if (file_name[j] == '.')
            {
                j++;
                for (int k = 0; k < 3 && j < file_name.length(); j++, k++)
                {
                    root_entry.extension[k] = file_name[j];
                }
            }
        }
    return root_entry;
}

void allocate_root_entry(Boot_Record boot_record, std::fstream &image_stream, Root_Entry root_entry, std::vector<unsigned int> &free_clusters, bool expand_root_dir)
{
    // Posições
    int inodes_position = boot_record.reserved_sectors * boot_record.bytes_per_sector + boot_record.bitmap_size * boot_record.bytes_per_sector;
    int data_position = boot_record.pos_data * boot_record.bytes_per_sector;
    // Variáveis
    int pointers_per_cluster = (boot_record.bytes_per_sector * boot_record.sectors_per_cluster) / 4 - 1;
    Inode new_root_dir, root_dir_inode;
    image_stream.seekg(inodes_position, std::ios::beg);
    image_stream.read((char *)&root_dir_inode, sizeof(Inode));
    new_root_dir = root_dir_inode;
    new_root_dir.size += 32;
    //
    int root_dir_clusters = std::ceil((float)new_root_dir.size / ((float)boot_record.bytes_per_sector * boot_record.sectors_per_cluster));
    int entries_per_cluster = (boot_record.bytes_per_sector * boot_record.sectors_per_cluster) / sizeof(Root_Entry);
    if (expand_root_dir)
    {
        int entry_loc = boot_record.pos_data * boot_record.bytes_per_sector + (free_clusters[0] - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
        image_stream.seekp(entry_loc, std::ios::beg);
        image_stream.write((char *)&root_entry, sizeof(Root_Entry));
        image_stream.flush();
        if (root_dir_clusters < 9)
        {
            new_root_dir.cluster_ptrs[root_dir_clusters - 1] = free_clusters[0];
        }
        else if (root_dir_clusters == 9)
        {
            new_root_dir.indirect_cluster_ptr = free_clusters[1];
            image_stream.seekp(data_position + (free_clusters[1] - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster, std::ios::beg);
            image_stream.write((char *)&free_clusters[0], 4);
            image_stream.flush();
        }
        else
        {
            unsigned int current_indirect_cluster = new_root_dir.indirect_cluster_ptr;
            bool allocated = false;
            while (!allocated)
            {
                int cluster;
                int ind_cluster_loc = boot_record.pos_data * boot_record.bytes_per_sector + (current_indirect_cluster - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
                image_stream.seekg(ind_cluster_loc, std::ios::beg);
                for (int i = 0; i < pointers_per_cluster && !allocated; i++)
                {
                    image_stream.read((char *)&cluster, 4);
                    if (cluster == 0x00)
                    {
                        image_stream.seekp((long int)image_stream.tellg() - 4, std::ios::beg);
                        image_stream.write((char *)&free_clusters[0], 4);
                        image_stream.flush();
                        allocated = true;
                    }
                }
                image_stream.read((char *)&current_indirect_cluster, 4);
            }
        }
    }
    else
    {
        int current_cluster = 0;
        int cluster;
        bool allocated = false;
        int current_indirect_cluster = new_root_dir.indirect_cluster_ptr;
        while (current_cluster < root_dir_clusters && !allocated)
        {
            if (current_cluster < 8)
            {
                for (; current_cluster < 8 && current_cluster < root_dir_clusters && !allocated; current_cluster++)
                {
                    cluster = new_root_dir.cluster_ptrs[current_cluster];
                    int cluster_loc = boot_record.pos_data * boot_record.bytes_per_sector + (cluster - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
                    image_stream.seekg(cluster_loc, std::ios::beg);
                    Root_Entry current_entry;
                    for (int j = 0; j < entries_per_cluster && !allocated; j++)
                    {
                        image_stream.read((char *)&current_entry, sizeof(Root_Entry));
                        if (current_entry.type == 0x00)
                        {
                            allocated = true;
                            image_stream.seekp((int)image_stream.tellg() - sizeof(Root_Entry), std::ios::beg);
                            image_stream.write((char *)&root_entry, sizeof(Root_Entry));
                            image_stream.flush();
                        }
                    }
                }
            }
            else
            {
                int ind_cluster_loc = boot_record.pos_data * boot_record.bytes_per_sector + (current_indirect_cluster - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
                image_stream.seekg(ind_cluster_loc, std::ios::beg);
                for (; current_cluster < pointers_per_cluster && current_cluster < root_dir_clusters && !allocated; current_cluster++)
                {
                    image_stream.read((char *)&cluster, 4);
                    image_stream.seekg(data_position + (cluster - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster, std::ios::beg);
                    Root_Entry current_entry;
                    for (int j = 0; j < entries_per_cluster && !allocated; j++)
                    {
                        image_stream.read((char *)&current_entry, sizeof(Root_Entry));
                        if (current_entry.type == 0x00)
                        {
                            allocated = true;
                            image_stream.seekp((int)image_stream.tellg() - sizeof(Root_Entry), std::ios::beg);
                            image_stream.write((char *)&root_entry, sizeof(Root_Entry));
                            image_stream.flush();
                        }
                    }
                }
                image_stream.read((char *)&current_indirect_cluster, 4);
            }
        }
    }
    image_stream.seekp(inodes_position, std::ios::beg);
    image_stream.write((char *)&new_root_dir, sizeof(Inode));
    image_stream.flush();
}

int need_new_root_dir_cluster(Boot_Record boot_record, std::fstream &image_stream)
{
    int new_amount_needed = 0;
    // Posições e Variáveis
    int bitmap_position = boot_record.reserved_sectors * boot_record.bytes_per_sector;
    int inodes_position = bitmap_position + boot_record.bitmap_size * boot_record.bytes_per_sector;
    int pointers_per_cluster = (boot_record.sectors_per_cluster * boot_record.bytes_per_sector) / 4 - 1;
    // Variável Inode do Diretório Raiz
    Inode root_dir_inode;
    image_stream.seekg(inodes_position, std::ios::beg);
    image_stream.read((char *)&root_dir_inode, sizeof(Inode));
    // Verifica se precisa de um novo cluster para o Diretório Raiz
    int root_dir_clusters_needed = std::ceil((float)root_dir_inode.size / ((float)boot_record.bytes_per_sector * boot_record.sectors_per_cluster));
    int new_root_dir_size = root_dir_inode.size + 32;
    root_dir_clusters_needed = std::max(1, root_dir_clusters_needed);
    int new_root_dir_clusters_needed = std::ceil((float)new_root_dir_size / ((float)boot_record.bytes_per_sector * boot_record.sectors_per_cluster));
    if (new_root_dir_clusters_needed > root_dir_clusters_needed)
        new_amount_needed++;
    // Verificar se precisa de um novo cluster indireto para o Diretório Raiz
    int old_indirect_needed = std::ceil((float)(std::max(root_dir_clusters_needed - 8, 0)) / (float)pointers_per_cluster);
    int new_indirect_needed = std::ceil((float)(std::max(new_root_dir_clusters_needed - 8, 0)) / (float)pointers_per_cluster);
    if (new_indirect_needed > old_indirect_needed)
        new_amount_needed++;
    return new_amount_needed;
}

void allocate_clusters(Boot_Record boot_record, std::fstream &image_stream, std::vector<unsigned int> &free_clusters)
{
    // Posições
    int bitmap_position = boot_record.reserved_sectors * boot_record.bytes_per_sector;
    // Alocação no Bitmap
    int cluster_index;
    for (cluster_index = 0; cluster_index < free_clusters.size(); cluster_index++)
    {
        int byte_loc = (free_clusters[cluster_index] - 1) / 8;
        uint bit = (free_clusters[cluster_index] - 1) % 8;
        image_stream.seekg(bitmap_position + byte_loc, std::ios::beg);
        char read_byte, new_byte;
        image_stream.read(&read_byte, 1);
        new_byte = read_byte | (128 >> bit);
        image_stream.seekp((int)image_stream.tellg() - 1, std::ios::beg);
        image_stream.write(&new_byte, 1);
        image_stream.flush();
    }
}

Inode create_file_inode(Boot_Record boot_record, std::fstream &image_stream, std::string file_name, std::fstream &file_stream, bool &success)
{
    int available_inodex_index = get_available_inode_index(boot_record, image_stream);
    Inode file_inode;
    if (!available_inodex_index)
    {
        success = false;
        return file_inode;
    }
    else
        success = true;
    file_inode.type = 0x01;
    file_inode.size = file_stream.tellg();
    file_inode.modification_time = static_cast<unsigned int>(time(0));
    file_inode.creation_time = static_cast<unsigned int>(time(0));
    for (int i = 0; i < 8; i++)
        file_inode.cluster_ptrs[i] = 0x00;
    file_inode.indirect_cluster_ptr = 0x00;
    return file_inode;
}

void copy_data(Boot_Record boot_record, std::fstream &image_stream, std::fstream &file_stream, std::vector<unsigned int> data_clusters)
{
    // Posições
    int data_position = boot_record.pos_data * boot_record.bytes_per_sector;
    file_stream.seekg(0, std::ios::end);
    int file_size = file_stream.tellg();
    file_stream.seekg(0, std::ios::beg);
    char *read_data = new char[boot_record.bytes_per_sector * boot_record.sectors_per_cluster];
    for (int i = 0; i < data_clusters.size() - 1; i++)
    {
        int data_cluster_loc = data_position + (data_clusters[i] - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
        image_stream.seekp(data_cluster_loc, std::ios::beg);
        file_stream.read(read_data, boot_record.bytes_per_sector * boot_record.sectors_per_cluster);
        image_stream.write(read_data, boot_record.bytes_per_sector * boot_record.sectors_per_cluster);
        image_stream.flush();
    }
    delete[] read_data;
    int data_left = file_size - (int)file_stream.tellg();
    int data_cluster_loc = data_position + (data_clusters[data_clusters.size() - 1] - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
    char *left_read_data = new char[data_left];
    image_stream.seekp(data_cluster_loc, std::ios::beg);
    file_stream.read(left_read_data, data_left);
    image_stream.write(left_read_data, data_left);
    image_stream.flush();
    delete[] left_read_data;
}

std::vector<unsigned int> allocate_inode(Boot_Record boot_record, std::fstream &image_stream, Inode inode, std::vector<unsigned int> &free_clusters, int data_clusters_needed)
{
    // Posições
    int inodes_position = boot_record.bytes_per_sector * boot_record.reserved_sectors + boot_record.bitmap_size * boot_record.bytes_per_sector;
    // Variáveis
    int pointers_per_cluster = (boot_record.sectors_per_cluster * boot_record.bytes_per_sector) / 4 - 1;
    std::vector<unsigned int> data_clusters;
    int clusters_used = 0;
    // Alocar Ponteiros Diretos
    for (int i = 0; i < 8 && i < data_clusters_needed; i++)
    {
        inode.cluster_ptrs[i] = free_clusters[i];
        data_clusters.push_back(free_clusters[i]);
        clusters_used++;
    }
    // Alocar Ponteiros Indiretos
    if (data_clusters_needed > 8)
    {
        int clusters_allocated = 8;
        clusters_used++;
        inode.indirect_cluster_ptr = free_clusters[clusters_allocated];
        int data_pos = boot_record.pos_data * boot_record.bytes_per_sector;
        while (clusters_allocated < data_clusters_needed - 1)
        {
            int ind_loc = data_pos + (free_clusters[clusters_allocated] - 1) * boot_record.sectors_per_cluster * boot_record.bytes_per_sector;
            image_stream.seekp(ind_loc, std::ios::beg);
            for (int i = 0; i < pointers_per_cluster && clusters_allocated < data_clusters_needed - 1; i++)
            {
                clusters_allocated++;
                image_stream.write((char *)(&free_clusters[clusters_allocated]), sizeof(int));
                image_stream.flush();
                data_clusters.push_back(free_clusters[clusters_allocated]);
                clusters_used++;
            }
            if (clusters_allocated < data_clusters_needed - 1)
            {
                clusters_allocated++;
                image_stream.write((char *)(&free_clusters[clusters_allocated]), sizeof(int));
                image_stream.flush();
                clusters_used++;
            }
        }
    }
    for (int i = 0; i < clusters_used; i++)
        free_clusters.erase(free_clusters.begin());
    // Write Inode
    image_stream.seekp(inodes_position + get_available_inode_index(boot_record, image_stream) * sizeof(Inode), std::ios::beg);
    image_stream.write((char *)&inode, sizeof(Inode));
    image_stream.flush();
    //
    return data_clusters;
}
bool copy_file_to_image(std::fstream &image_stream, std::string file_name, std::fstream &file_stream)
{
    // Ler Boot Record
    Boot_Record boot_record;
    image_stream.seekg(0, std::ios::beg);
    image_stream.read((char *)&boot_record, sizeof(Boot_Record));
    // Variáveis
    file_stream.seekg(0, std::ios::end);
    int file_size = file_stream.tellg();
    int data_clusters_needed;
    bool reallocate_root = false;
    bool success = false;
    // Cálculo Clusters Necessários
    int clusters_needed = std::ceil((float)file_size / (float)(boot_record.sectors_per_cluster * boot_record.bytes_per_sector));
    clusters_needed += std::ceil((float)(clusters_needed - 8) / (float)((boot_record.sectors_per_cluster * boot_record.bytes_per_sector) / 4 - 1));
    int root_dir_clusters_needed = need_new_root_dir_cluster(boot_record, image_stream);
    data_clusters_needed = clusters_needed;
    clusters_needed += root_dir_clusters_needed;
    reallocate_root = root_dir_clusters_needed;
    // Verificar Clusters Disponíveis
    std::vector<unsigned int> free_clusters = find_clusters(boot_record, image_stream, clusters_needed, success);
    if (!success)
    {
        std::cout << "Não há clusters disponíveis para alocar o arquivo\n";
        return false;
    }
    // Alocar Clusters no Bitmap
    allocate_clusters(boot_record, image_stream, free_clusters);
    // Criar Inode
    Inode file_inode = create_file_inode(boot_record, image_stream, file_name, file_stream, success);
    if (!success)
    {
        std::cout << "Não há inodes disponíveis\n";
        return false;
    }
    // Criar Entrada de Diretório
    Root_Entry file_entry = create_root_entry(boot_record, image_stream, file_name);
    Inode root_dir_inode;
    // Alocar Inode
    std::vector<unsigned int> data_clusters = allocate_inode(boot_record, image_stream, file_inode, free_clusters, data_clusters_needed);
    // Alocar Entrada e Realocar Diretório Raiz (Se Necessário)
    allocate_root_entry(boot_record, image_stream, file_entry, free_clusters, reallocate_root);
    // Fazer transferência para os clusters de dados
    copy_data(boot_record, image_stream, file_stream, data_clusters);
    return true;
}

#endif
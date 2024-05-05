#ifndef READER_HH
#define READER_HH

#include <vector>
#include <iostream>
#include <fstream>
#include <cmath>
#include "rfstruct.hh"

std::vector<int> get_clusters(Boot_Record boot_record, std::fstream &image_stream, Inode inode)
{
    // Posições e Variáveis
    int data_position = boot_record.pos_data * boot_record.bytes_per_sector;
    int pointers_per_cluster = ((boot_record.sectors_per_cluster * boot_record.bytes_per_sector) / 4) - 1;
    int cluster_amount = std::ceil((float)inode.size / (float)(boot_record.bytes_per_sector * boot_record.sectors_per_cluster));
    cluster_amount = std::max(1, cluster_amount);
    cluster_amount += std::ceil((float)cluster_amount / (float)pointers_per_cluster);
    std::vector<int> clusters;
    // pegar os ponteiros diretos
    for (int i = 0; i < 8; i++)
    {
        if (inode.cluster_ptrs[i] == 0x00)
            break;
        clusters.push_back(inode.cluster_ptrs[i]);
    }

    int indirect_ptr = inode.indirect_cluster_ptr;
    int clusters_read = 8;
    while (clusters_read < cluster_amount - 1)
    {
        image_stream.seekg(data_position + (indirect_ptr - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster, std::ios::beg);
        unsigned int pointer;
        for (int i = 0; i < pointers_per_cluster && clusters_read < cluster_amount - 1; i++)
        {
            clusters_read++;
            image_stream.read((char *)&pointer, 4);
            clusters.push_back(pointer);
        }
        if (clusters_read < cluster_amount - 1)
        {
            clusters_read++;
            image_stream.read((char *)&indirect_ptr, 4);
        }
    }
    return clusters;
}

std::vector<Root_Entry> get_root_entries(Boot_Record boot_record, std::fstream &image_stream, Inode root_dir_inode)
{
    // Posições
    int data_position = boot_record.pos_data * boot_record.bytes_per_sector;
    //
    std::vector<Root_Entry> entries;
    std::vector<int> root_dir_clusters = get_clusters(boot_record, image_stream, root_dir_inode);
    for (int i = 0; i < root_dir_clusters.size(); i++)
    {
        int cluster_loc = data_position + (root_dir_clusters[i] - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
        image_stream.seekg(cluster_loc, std::ios::beg);
        Root_Entry entry;
        int entries_per_cluster = (boot_record.sectors_per_cluster * boot_record.bytes_per_sector) / sizeof(Root_Entry);
        for (int j = 0; j < entries_per_cluster; j++)
        {
            image_stream.read((char *)(&entry), sizeof(Root_Entry));
            if (entry.type == 0x10)
                entries.push_back(entry);
        }
    }
    return entries;
}

void list_entries(std::fstream &image_stream)
{
    // Boot Record
    Boot_Record boot_record;
    image_stream.seekg(0, std::ios::beg);
    image_stream.read((char *)&boot_record, sizeof(Boot_Record));
    // Posições e Variáveis
    int inodes_position = boot_record.bytes_per_sector * (boot_record.bitmap_size + boot_record.reserved_sectors);
    Inode root_dir_inode;
    image_stream.seekg(inodes_position, std::ios::beg);
    image_stream.read((char *)&root_dir_inode, sizeof(Inode));
    //
    std::vector<Root_Entry> entries = get_root_entries(boot_record, image_stream, root_dir_inode);
    std::cout << "=== Diretório Raiz ===" << '\n';
    for (int i = 0; i < entries.size(); i++)
    {
        std::cout << i + 1 << " --- ";
        for (int j = 0; j < 20 && entries[i].name[j] != 0x00; j++)
            std::cout << entries[i].name[j];
        std::cout << '.';
        for (int j = 0; j < 3 && entries[i].extension != 0x00; j++)
            std::cout << entries[i].extension[j];
        std::cout << '\n';
    }
    std::cout << '\n';
}

bool copy_file_from_image(std::fstream &image_stream, std::string file_name, std::string path)
{
    // Boot Record
    Boot_Record boot_record;
    image_stream.seekg(0, std::ios::beg);
    image_stream.read((char *)&boot_record, sizeof(Boot_Record));
    // Posições e Variáveis
    int inodes_position = boot_record.bytes_per_sector * (boot_record.bitmap_size + boot_record.reserved_sectors);
    int data_position = boot_record.pos_data * boot_record.bytes_per_sector;
    Inode root_dir_inode;
    image_stream.seekg(inodes_position, std::ios::beg);
    image_stream.read((char *)&root_dir_inode, sizeof(Inode));
    //
    std::vector<Root_Entry> entries = get_root_entries(boot_record, image_stream, root_dir_inode);
    Root_Entry entry;
    bool found_entry = false;
    for (int i = 0; i < entries.size() && !found_entry; i++)
    {
        found_entry = true;
        entry = entries[i];
        std::string entry_name;
        for (int j = 0; j < 20 && entries[i].name[j] != 0x00 && found_entry; j++)
            entry_name.append(1, entries[i].name[j]);
        entry_name.append(1, '.');
        for (int j = 0; j < 3 && entries[i].extension[j] != 0x00 && found_entry; j++)
            entry_name.append(1, entries[i].extension[j]);
        if (entry_name != file_name)
            found_entry = false;
    }
    if (!found_entry)
    {
        std::cout << "Arquivo inexistente" << '\n';
        return false;
    }
    if (path.size() != 0 && path[path.size() - 1] != '/')
        path.append("/");
    path.append(file_name);

    std::ofstream file_stream(path, std::ios::binary);
    Inode inode;
    image_stream.seekg(inodes_position + entry.inode_id * sizeof(Inode), std::ios::beg);
    image_stream.read((char *)&inode, sizeof(Inode));
    std::vector<int> clusters = get_clusters(boot_record, image_stream, inode);
    file_stream.seekp(0, std::ios::beg);
    char *buffer = new char[boot_record.bytes_per_sector * boot_record.sectors_per_cluster];
    for (int i = 0; i < clusters.size() - 1; i++)
    {
        int cluster_loc = data_position + (clusters[i] - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
        image_stream.seekg(cluster_loc, std::ios::beg);
        image_stream.read(buffer, boot_record.bytes_per_sector * boot_record.sectors_per_cluster);
        image_stream.flush();
        file_stream.write(buffer, boot_record.bytes_per_sector * boot_record.sectors_per_cluster);
        file_stream.flush();
    }
    delete[] buffer;
    int file_size = inode.size;
    int data_left = file_size - (boot_record.bytes_per_sector * boot_record.sectors_per_cluster * (clusters.size() - 1));
    char *left_buffer = new char[data_left];
    image_stream.seekg(data_position + (clusters[clusters.size() - 1] - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster, std::ios::beg);
    image_stream.read(left_buffer, data_left);
    image_stream.flush();
    file_stream.write(left_buffer, data_left);
    file_stream.flush();
    delete[] left_buffer;
    return true;
}

bool read_file_from_image(std::fstream &image_stream, std::string file_name)
{
    // Boot Record
    Boot_Record boot_record;
    image_stream.seekg(0, std::ios::beg);
    image_stream.read((char *)&boot_record, sizeof(Boot_Record));
    // Posições e Variáveis
    int inodes_position = boot_record.bytes_per_sector * (boot_record.bitmap_size + boot_record.reserved_sectors);
    int data_position = boot_record.pos_data * boot_record.bytes_per_sector;
    Inode root_dir_inode;
    image_stream.seekg(inodes_position, std::ios::beg);
    image_stream.read((char *)&root_dir_inode, sizeof(Inode));
    //
    std::vector<Root_Entry> entries = get_root_entries(boot_record, image_stream, root_dir_inode);
    Root_Entry entry;
    bool found_entry = false;
    for (int i = 0; i < entries.size() && !found_entry; i++)
    {
        found_entry = true;
        entry = entries[i];
        std::string entry_name;
        for (int j = 0; j < 20 && entries[i].name[j] != 0x00 && found_entry; j++)
            entry_name.append(1, entries[i].name[j]);
        entry_name.append(1, '.');
        for (int j = 0; j < 3 && entries[i].extension[j] != 0x00 && found_entry; j++)
            entry_name.append(1, entries[i].extension[j]);
        if (entry_name != file_name)
            found_entry = false;
    }
    if (!found_entry)
    {
        std::cout << "Arquivo inexistente" << '\n';
        return false;
    }
    //
    Inode inode;
    image_stream.seekg(inodes_position + entry.inode_id * sizeof(Inode), std::ios::beg);
    image_stream.read((char *)&inode, sizeof(Inode));
    std::vector<int> clusters = get_clusters(boot_record, image_stream, inode);
    for (int i = 0; i < clusters.size() - 1; i++)
    {
        int cluster_loc = data_position + (clusters[i] - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster;
        image_stream.seekg(cluster_loc, std::ios::beg);
        char buffer[boot_record.bytes_per_sector * boot_record.sectors_per_cluster];
        image_stream.read(buffer, boot_record.bytes_per_sector * boot_record.sectors_per_cluster);
        for (int j = 0; j < boot_record.bytes_per_sector * boot_record.sectors_per_cluster; j++)
            std::cout << buffer[j];
    }
    int file_size = inode.size;
    int data_left = file_size - (boot_record.bytes_per_sector * boot_record.sectors_per_cluster * (clusters.size() - 1));
    char buffer[data_left];
    image_stream.seekg(data_position + (clusters[clusters.size() - 1] - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster, std::ios::beg);
    image_stream.read(buffer, data_left);
    for (int i = 0; i < data_left; i++)
        std::cout << buffer[i];
    std::cout << '\n';
    return true;
}

#endif
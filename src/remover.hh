#ifndef REMOVER_HH
#define REMOVER_HH

#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <cmath>
#include "rfstruct.hh"

std::vector<int> get_clusters_for_removal(Boot_Record boot_record, std::fstream &image_stream, Inode inode)
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

    if (cluster_amount > 8)
    {
        int indirect_ptr = inode.indirect_cluster_ptr;
        clusters.push_back(indirect_ptr);
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
                clusters.push_back(indirect_ptr);
            }
        }
    }
    return clusters;
}

std::vector<int> get_clusters_special(Boot_Record boot_record, std::fstream &image_stream, Inode inode)
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

std::vector<Root_Entry> get_root_entries_for_removal(Boot_Record boot_record, std::fstream &image_stream, Inode root_dir_inode,
    std::vector<int> &entries_clusters, std::vector<int> &entries_clusters_offset)
{
    // Posições
    int data_position = boot_record.pos_data * boot_record.bytes_per_sector;
    //
    std::vector<Root_Entry> entries;
    std::vector<int> root_dir_clusters = get_clusters_special(boot_record, image_stream, root_dir_inode);
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
            {
                entries.push_back(entry);
                entries_clusters.push_back(root_dir_clusters[i]);
                entries_clusters_offset.push_back(j);
            }
        }
    }
    return entries;
}

bool erase_image_file(std::fstream &image_stream, std::string file_name)
{
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
    std::vector<int> entries_clusters, entries_clusters_offset;
    std::vector<Root_Entry> entries = get_root_entries_for_removal(boot_record, image_stream, root_dir_inode, entries_clusters, entries_clusters_offset);
    Root_Entry entry;
    bool found_entry = false;
    int i;
    for (i = 0; i < entries.size() && !found_entry; i++)
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
    // Desalocar Inode
    Inode inode;
    image_stream.seekg(inodes_position + entry.inode_id * sizeof(Inode), std::ios::beg);
    image_stream.read((char *)&inode, sizeof(Inode));
    inode.type = 0x00;
    std::vector<int> inode_clusters = get_clusters_for_removal(boot_record, image_stream, inode);
    image_stream.seekp(inodes_position + entry.inode_id * sizeof(Inode), std::ios::beg);
    image_stream.write((char *)&inode, sizeof(Inode));
    image_stream.flush();
    // Remover Entrada no Diretório
    entry.type = 0x00;
    image_stream.seekp(data_position + (entries_clusters[i - 1] - 1) * boot_record.bytes_per_sector * boot_record.sectors_per_cluster + entries_clusters_offset[i - 1] * sizeof(Root_Entry), std::ios::beg);
    image_stream.write((char *)&entry, sizeof(Root_Entry));
    image_stream.flush();
    // Desalocar Clusters
    int bitmap_position = boot_record.reserved_sectors * boot_record.bytes_per_sector;
    int cluster_index;
    for (cluster_index = 0; cluster_index < inode_clusters.size(); cluster_index++)
    {
        int byte_loc = (inode_clusters[cluster_index] - 1) / 8;
        uint bit = (inode_clusters[cluster_index] - 1) % 8;
        image_stream.seekg(bitmap_position + byte_loc, std::ios::beg);
        char read_byte, new_byte;
        image_stream.read(&read_byte, 1);
        new_byte = read_byte & ~(128 >> bit);
        image_stream.seekp((int)image_stream.tellg() - 1, std::ios::beg);
        image_stream.write(&new_byte, 1);
        image_stream.flush();
    }
    return true;
}

#endif
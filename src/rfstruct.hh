#ifndef RFSTRUCT_HH
#define RFSTRUCT_HH

typedef struct Boot_Record
{
    unsigned short int bytes_per_sector;
    char sectors_per_cluster;
    char reserved_sectors;
    unsigned long int total_sectors;
    unsigned long int bitmap_size;
    unsigned long int inodes;
    unsigned int len_inode;
    unsigned long int pos_data;
} __attribute__((packed)) Boot_Record;

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

typedef struct Root_Entry
{
    char type;
    unsigned long int inode_id;
    char name[20];
    char extension[3];
} __attribute__((packed)) Root_Entry;

#endif
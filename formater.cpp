#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
#include <cstdint>

#define BYTES_PER_SECTOR 512

using namespace std;

typedef struct bootRecord
{
    unsigned short int BytesPerSector;
    char SectorsPerCluster;
    char ReservedSectors;
    unsigned long int TotalOfSectors;
    unsigned long int SizeOfBitMap;
    unsigned long int Inodes;
    unsigned short int LenOfInode;
    unsigned long int PosData;
} __attribute__((packed)) bootRecord;

typedef struct Inode 
{
    char Type;
    char Alignment[3];
    unsigned long int Size;
    unsigned long int ModificationTime;
    unsigned long int CreationTime;
    unsigned int Clusters[8];
    unsigned int IndirectCluster;
} __attribute__((packed)) Inode;

bootRecord define_br(unsigned long int size_of_img, unsigned int sector_per_cluster)
{
    bootRecord br;
    br.BytesPerSector = BYTES_PER_SECTOR;
    br.SectorsPerCluster = sector_per_cluster;
    br.LenOfInode = 64; // len(In0de)
    br.TotalOfSectors = size_of_img;
    br.ReservedSectors = 1;

    size_of_img--;

    float TotalAgrupamentos = (br.TotalOfSectors / (br.LenOfInode * 8 + (br.BytesPerSector * 8 * br.SectorsPerCluster) + 1));
    br.SizeOfBitMap = int(TotalAgrupamentos);
    br.Inodes = int(TotalAgrupamentos) * BYTES_PER_SECTOR * 8;

    int v_exedente = size_of_img - int(TotalAgrupamentos) * (br.LenOfInode * 8 + (br.BytesPerSector * 8 * br.SectorsPerCluster) + 1);

    if (v_exedente >= (1 + 1 + 8 * br.SectorsPerCluster))
    {
        br.SizeOfBitMap++;
        br.Inodes += BYTES_PER_SECTOR / br.LenOfInode;
        v_exedente -= 2;
        if (v_exedente % br.SectorsPerCluster)
            br.ReservedSectors += v_exedente % br.SectorsPerCluster;
    }
    else
        br.ReservedSectors += v_exedente;

    br.PosData = br.ReservedSectors + br.SizeOfBitMap + (br.Inodes / (br.BytesPerSector / br.LenOfInode));


    return br;
}

void clean_ghost_clusters(const string &filename, const bootRecord &BR)
{
    fstream file(filename, ios::binary | ios::out | ios::in);

    int disponiveis = BR.TotalOfSectors - BR.PosData;
    cout << disponiveis << endl;

    file.seekp(BR.ReservedSectors * BR.BytesPerSector + disponiveis / 8 - 1, ios::beg);

    uint8_t byte = 0xFF;
    short resto = disponiveis % 8;
    byte >>= resto;

    file.write(reinterpret_cast<char *>(&byte), sizeof(uint8_t));

    byte = -1;
    for (streampos p = file.tellp(); p < (BR.ReservedSectors + BR.SizeOfBitMap) * BR.BytesPerSector; p+=1)
    {
        file.write(reinterpret_cast<char *>(&byte), sizeof(byte));
    }
}

void create_root_dir(const string &filename, const bootRecord &BR)
{
    fstream file(filename, ios::binary | ios::out | ios::in);

    Inode inode;
    //char Type;
    inode.Type = 0x01;
    //char Alignment[3];
    //unsigned long int Size;
    inode.Size = 0;
    //unsigned int ModificationTime;
    time_t now = time(0);
    inode.ModificationTime = static_cast<unsigned int>(now);
    //unsigned int CreationTime;
    inode.CreationTime = static_cast<unsigned int>(now);
    //unsigned int Clusters[8];
    inode.Clusters[0] = 1;
    for (int i = 1; i < 8; i++)
    {
        inode.Clusters[i] = 0;
    }
    // unsigned int IndirectCluster;
    inode.IndirectCluster = 0;

    file.seekp((BR.ReservedSectors) * BR.BytesPerSector, ios::beg);
    int8_t byte = 0x80;
    file.write(reinterpret_cast<const char *>(&byte), sizeof(byte));
    file.seekp(0, ios::cur);
    file.seekp((BR.ReservedSectors + BR.SizeOfBitMap) * BR.BytesPerSector, ios::beg);
    file.write(reinterpret_cast<const char *>(&inode), sizeof(Inode));
}

void write_br_to_file(const string &filename, const bootRecord &BR)
{
    fstream file(filename, ios::binary | ios::out | ios::in);
    if (!file)
    {
        cout << "Erro ao abrir o arquivo" << endl;
        return;
    }

    // Posicionar o ponteiro de escrita no início do arquivo
    file.seekp(0, ios::beg);

    // Escrever o bootRecord no arquivo
    file.write(reinterpret_cast<const char *>(&BR), sizeof(bootRecord));

    // Fechar o arquivo
    file.close();
}

bootRecord read_br_from_file(const string &filename)
{
    bootRecord BR;

    // Abrir o arquivo em modo binário de leitura
    ifstream file(filename, ios::binary);
    if (!file)
    {
        cout << "Erro ao abrir o arquivo" << endl;
        return BR;
    }

    // Ler o bootRecord do arquivo
    file.read(reinterpret_cast<char *>(&BR), sizeof(bootRecord));

    // Fechar o arquivo
    file.close();

    return BR;
}

void print_br(const bootRecord &BR)
{
    cout << "BytesPerSector: " << BR.BytesPerSector << endl;
    cout << "SectorsPerCluster: " << static_cast<int>(BR.SectorsPerCluster) << endl;
    cout << "ReservedSectors: " << (int)BR.ReservedSectors << endl;
    cout << "TotalOfSectors: " << BR.TotalOfSectors << endl;
    cout << "SizeOfBitMap: " << BR.SizeOfBitMap << endl;
    cout << "Inodes: " << BR.Inodes << endl;
    cout << "LenOfInode: " << BR.LenOfInode << endl;
    cout << "PosData: " << BR.PosData << endl;
}

int main()
{
    /*
    TO - DO:
        -- bitmap clusters fantasmas ✅
        -- criar o Inode do root dir ✅
     */

    long int size_of_img = 546;
    unsigned int sector_per_cluster = 1;
    string name_img("teste.img");

    std::cout << "Qual o número Setores? ";
    std::cin >> size_of_img;
    std::cout << "Qual o número Setores per Cluster? (1) ";
    std::cin >> sector_per_cluster;
    // std::cout << " Nome da imagem: ";
    // cin >> name_img;

    fstream file(name_img, ios::binary | ios::app);
    if (!file)
    {
        cout << "Arquivo não encontrado, criando o arquivo" << endl;
    }

    for (int i = 0; i < size_of_img; i++)
    {
        char padding[512] = {0};
        file.write(padding, 512);
    }

    bootRecord BR = define_br(size_of_img, sector_per_cluster);

    // Escrever o bootRecord no arquivo
    write_br_to_file(name_img, BR);
    clean_ghost_clusters(name_img, BR);
    create_root_dir(name_img, BR);

    // Ler o bootRecord do arquivo
    bootRecord read_BR = read_br_from_file(name_img);

    // Exibir o bootRecord lido
    cout << "Boot Record lido do arquivo:" << endl;
    print_br(read_BR);

    return 0;
}

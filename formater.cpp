#include <iostream>
#include <fstream>
#include <string>

#define BYTES_PER_SECTOR 512

using namespace std;

typedef struct bootRecord
{
    unsigned short int BytesPerSector;
    char SectorsPerCluster;
    unsigned short int ReservedSectors;
    unsigned long int TotalOfSectors;
    unsigned long int SizeOfBitMAp;
    unsigned long int INODES;
    unsigned short int LenOfInode;
} __attribute__((packed)) bootRecord;

bootRecord define_br(unsigned long int size_of_img, unsigned char sector_per_cluster)
{
    bootRecord BR;
    BR.BytesPerSector = BYTES_PER_SECTOR;
    BR.SectorsPerCluster = sector_per_cluster;
    BR.LenOfInode = 32; // len(In0de)
    BR.TotalOfSectors = size_of_img;
    BR.ReservedSectors = 1;

    size_of_img--;

    int total_agrupamentos = BR.TotalOfSectors / (BR.LenOfInode + (BR.BytesPerSector * BR.SectorsPerCluster) + 1);
    BR.SizeOfBitMAp = total_agrupamentos;
    BR.INODES = total_agrupamentos * BR.LenOfInode * (BYTES_PER_SECTOR / BR.LenOfInode);

    return BR;
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
    cout << "ReservedSectors: " << BR.ReservedSectors << endl;
    cout << "TotalOfSectors: " << BR.TotalOfSectors << endl;
    cout << "SizeOfBitMAp: " << BR.SizeOfBitMAp << endl;
    cout << "INODES: " << BR.INODES << endl;
    cout << "LenOfInode: " << BR.LenOfInode << endl;
}

int main()
{
    long int size_of_img = 546;
    unsigned char sector_per_cluster = 1;
    string name_img("teste.img");

    std::cout << "Qual o número Setores? ";
    std::cin >> size_of_img;
    // std::cout << "Qual o número Setores per Cluster? (1) ";
    // std::cin >> sector_per_cluster;
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

    // Ler o bootRecord do arquivo
    bootRecord read_BR = read_br_from_file(name_img);

    // Exibir o bootRecord lido
    cout << "Boot Record lido do arquivo:" << endl;
    print_br(read_BR);

    return 0;
}

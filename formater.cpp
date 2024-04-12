#include <iostream>
#include <fstream>
#include <string>
#include <cmath>

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
    unsigned long int PosData;
} __attribute__((packed)) bootRecord;

bootRecord define_br(unsigned long int size_of_img, unsigned int sector_per_cluster)
{
    bootRecord br;
    br.BytesPerSector = BYTES_PER_SECTOR;
    br.SectorsPerCluster = sector_per_cluster;
    br.LenOfInode = 64; // len(In0de)
    br.TotalOfSectors = size_of_img;
    br.ReservedSectors = 1;


    size_of_img--;

    /* 
        4096 total de bit em um setor de 512 bytes

        4096 * len(inode) / 512


    
    
     */

    float total_agrupamentos = (br.TotalOfSectors / ( br.LenOfInode * 8 + (br.BytesPerSector * 8 * br.SectorsPerCluster) + 1));
    br.SizeOfBitMAp = int(total_agrupamentos);
    br.INODES = int(total_agrupamentos) * BYTES_PER_SECTOR * 8 ;



    int v_execente = size_of_img - int(total_agrupamentos) * (br.LenOfInode * 8 + (br.BytesPerSector * 8 * br.SectorsPerCluster) + 1);

    if (v_execente >= (1 + 1 + 8 * sector_per_cluster))
    {
        br.SizeOfBitMAp++;
        br.INODES += BYTES_PER_SECTOR / br.LenOfInode;
        v_execente -= 2;
        if (v_execente % sector_per_cluster)
            br.ReservedSectors += v_execente % sector_per_cluster;
    }
    else
        br.ReservedSectors += v_execente;

    br.PosData = br.ReservedSectors + br.SizeOfBitMAp + (br.INODES/(br.BytesPerSector/br.LenOfInode));

    return br;
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
    /* 
    TO - DO:
        -- bitmap clusters fantasmas 
        -- criar o Inode do root dir
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

    // Ler o bootRecord do arquivo
    bootRecord read_BR = read_br_from_file(name_img);

    // Exibir o bootRecord lido
    cout << "Boot Record lido do arquivo:" << endl;
    print_br(read_BR);

    return 0;
}

#include <iostream>
#include <stdio.h>
#include <vector>
#include <string>
#include <fstream>
#include "rfstruct.hh"
#include "reader.hh"
#include "writer.hh"
#include "remover.hh"

// Variáveis Globais

std::string image_path, image_name; // Caminho para imagem
std::fstream image_stream;          // File Handler da imagem

// Funções

std::string get_file_name(std::string path)
{
    std::string file_name;
    for (int i = path.size() - 1; i != -1 && path[i] != '/'; i--)
        file_name.insert(file_name.begin(), path[i]);
    return file_name;
}

int read_image(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cout << "Caminho da imagem não especificado. Programa encerrado.\n";
        return 0;
    }
    else
    {
        image_path = argv[1];
        image_stream.open(image_path, std::ios::binary | std::ios::in | std::ios::out);
        if (!image_stream)
        {
            std::cout << "Caminho da imagem não encontrado ou não existe. Programa encerrado.\n";
            return 0;
        }
    }
    image_name = get_file_name(image_path);
    return 1;
}

void print_intro()
{
    std::cout << "=== Imagem ROTOM '" << image_name << "' ==="
              << "\n\n";
}

void print_commands()
{
    std::cout << "== Comandos ==" << '\n';
    std::cout << "--- imove | Cópia de um arquivo para dentro da imagem" << '\n';
    std::cout << "--- emove | Cópia de um arquivo contido na imagem para o sistema externo" << '\n';
    std::cout << "--- read | Leitura de um arquivo contido na imagem" << '\n';
    std::cout << "--- delete | Remoção de um arquivo contido na imagem" << '\n';
    std::cout << "--- list | Listagem dos arquivos contidos dentro da imagem" << '\n';
    std::cout << "--- info | Listagem das informações sobre a imagem" << '\n';
    std::cout << "--- help | Listagem dos comandos" << '\n';
    std::cout << "--- exit | Encerra o programa" << '\n';
}

void show_info()
{
    image_stream.seekg(std::ios::beg);
    Boot_Record boot_record;
    image_stream.read((char *)&boot_record, sizeof(Boot_Record));
    print_intro();
    std::cout << "Setores reservados: " << (int)boot_record.reserved_sectors << '\n';
    std::cout << "Quantidade total de setores: " << boot_record.total_sectors << '\n';
    std::cout << "Bytes por setor: " << (int)boot_record.bytes_per_sector << '\n';
    std::cout << "Setores por clusters: " << (int)boot_record.sectors_per_cluster << '\n';
    std::cout << "Tamanho do bitmap em setores: " << boot_record.bitmap_size << '\n';
    std::cout << "Quantidade máxima de Inode: " << boot_record.inodes << '\n';
    std::cout << "Tamanho do Inode: " << boot_record.len_inode << '\n';
    std::cout << "Posição incial dos dados em setores: " << boot_record.pos_data << '\n';
}

void internal_file_move()
{
    std::string moving_file_path, moving_file_name;
    std::fstream moving_file_stream;
    std::cout << "Insira o caminho do arquivo a ser copiado" << '\n';
    std::getline(std::cin, moving_file_path);
    moving_file_name = get_file_name(moving_file_path);
    moving_file_stream = std::fstream(moving_file_path, std::ios::binary | std::ios::in | std::ios::out);
    if (!moving_file_stream)
        std::cout << "Caminho do arquivo não encontrado ou não existe.\n";
    else
    {
        if (copy_file_to_image(image_stream, moving_file_name, moving_file_stream))
            std::cout << "Arquivo copiado com sucesso" << '\n';
        else
            std::cout << "Cópia de arquivo falhou" << '\n';
    }
    moving_file_stream.close();
}

void external_file_move()
{
    std::string file_name, path;
    std::cout << "Insira o nome do arquivo a ser copiado para o sistema externo" << '\n';
    std::getline(std::cin, file_name);
    std::cout << "Insira o caminho para onde o arquivo será copiado" << '\n';
    std::getline(std::cin, path);
    if (copy_file_from_image(image_stream, file_name, path))
        std::cout << "Arquivo copiado com sucesso" << '\n';
    else
        std::cout << "Falha ao copiar um arquivo" << '\n';
}

void read_file()
{
    std::string file_name;
    std::cout << "Insira o nome do arquivo a ser lido, será adicionado um \\n ao final, independentemente do conteúdo" << '\n';
    std::getline(std::cin, file_name);
    if (!read_file_from_image(image_stream, file_name))
        std::cout << "Falha ao ler um arquivo" << '\n';
}

void delete_file()
{
    std::string file_name;
    std::cout << "Insira o nome do arquivo a ser excluído. Aviso! Esta operação consiste de desalocações, e potencialmente não remove os traços (sobrescreve) os seus dados" << '\n';
    std::getline(std::cin, file_name);
    if (!erase_image_file(image_stream, file_name))
        std::cout << "Falha ao excluir um arquivo" << '\n';
}

// Função Principal

int main(int argc, char *argv[])
{
    std::string input;
    if (read_image(argc, argv))
    {
        print_intro();
        print_commands();
        while (input != "exit")
        {
            std::cout << '>';
            std::getline(std::cin, input);
            if (input == "help")
                print_commands();
            else if (input == "imove")
                internal_file_move();
            else if (input == "emove")
                external_file_move();
            else if (input == "read")
                read_file();
            else if (input == "delete")
                delete_file();
            else if (input == "list")
                list_entries(image_stream);
            else if (input == "info")
                show_info();
            else if (input != "exit")
                std::cout << "Comando '" << input << "' não existe. Tente 'help'\n";
        }
    }
}
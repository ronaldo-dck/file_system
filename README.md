# Implementação do Sistema de Arquivos ROTOM

**Possui dois programas principais, o formatador e o manipulador.**

O código fonte do formatador é o formater.cpp, já o do manipulador é o rotom.cc.

É possível compilar ambos códigos por meio de um compilador de C++, utilize o GCC/G++ da seguinte forma, pelo terminal:

    g++ formater.cpp -o formater.x

    g++ rotom.cc -o rotom.x

Utilize os arquivos .x para executar os programas, da seguinte forma, no terminal:

    ./formater.x

    ./rotom.x

## Formatador

O formatador é responsável por criar uma imagem (arquivo binário) com uma quantidade
arbitrária de setores tal como uma quantidade arbitrária de setores por cluster.

Para utilizar o formatador, execute o arquivo executável .x compilado do arquivo formater.cpp.

Será questionado no terminal com quantos setores deseja gerar a imagem, e quantos setores por cluster ela terá. Após inserido tais entradas será gerado um arquivo *teste.img*.

O seguinte fluxo de terminal demonstra o funcionamento do formatador.

    ./formater.x
    Qual o número Setores? 200000
    Qual o número Setores per Cluster? (1) 4
    194352
    Boot Record lido do arquivo:
    BytesPerSector: 512
    SectorsPerCluster: 4
    ReservedSectors: 3
    TotalOfSectors: 200000
    SizeOfBitMap: 12
    Inodes: 45064
    LenOfInode: 64
    PosData: 5648

O arquivo *teste.img* possuirá as características descritas no resultado do programa, como mostra acima.

## Manipulador

O manipulador é responsável por realizar diversas operações sobre uma imagem no formato definido no Sistema de Arquivos Rotom.

Essas operações incluem cópia de um arquivo do sistema externo para imagem, cópia de um arquivo contido na imagem para o sistema externo, leitura de um arquivo contido na imagem, deletar um arquivo contido na imagem conforme descrito na especificação do sistema Rotom assim como mostrar informações a respeito da imagem.

Ao executar o arquivo .x compilado do arquivo rotom.cc você será informado de diferentes comandos, todos com funcionalidade descrita na tela. Por exemplo, para ler um arquivo utilizamos a opção read, que como descrito, realiza a leitura de um arquivo contido na imagem.

O seguinte fluxo descreve a utilização do manipulador para copiar um arquivo do sistema externo para a imagem, utilizando o terminal.

    ./rotom.x

    === Imagem ROTOM 'teste.img' ===

    == Comandos ==
    --- imove | Cópia de um arquivo para dentro da imagem
    --- emove | Cópia de um arquivo contido na imagem para o sistema externo
    --- read | Leitura de um arquivo contido na imagem
    --- delete | Remoção de um arquivo contido na imagem
    --- list | Listagem dos arquivos contidos dentro da imagem
    --- info | Listagem das informações sobre a imagem
    --- help | Listagem dos comandos
    --- exit | Encerra o programa
    >imove
    Insira o caminho do arquivo a ser copiado
    rotom.cc
    Arquivo copiado com sucesso.
    >list
    === Diretório Raiz ===
    1 --- rotom.cc

    >

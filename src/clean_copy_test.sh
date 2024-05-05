rm -rf teste.img

g++ formater.cpp -o formater.x

g++ copy.cpp -o copy.x

./formater.x

./copy.x teste.img copy.cpp
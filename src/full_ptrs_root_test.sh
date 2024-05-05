./teste.sh

g++ copy.cpp -o copy.x

for i in {1..8};
    do ./fill_entries_test.sh;
done;

./copy.x teste.img copy.cpp
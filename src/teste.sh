#!/bin/bash

rm -rf teste.img

g++ formater.cpp -o formater.x

g++ rotom.cc -o rotom.x

./formater.x

./rotom.x teste.img
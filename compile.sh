#!/bin/bash

sudo ./build-rpi3-arm64.sh;
sudo ./scripts/mkbootimg_rpi3.sh;
sudo mv boot.img modules.img ../tizen-image;
sudo umount ../tizen-image/rootfs.img ../mnt_dir;
sudo mount ../tizen-image/rootfs.img ../mnt_dir;
cd test; arm-linux-gnueabi-gcc -I../include selector.c -o selector; cd ..;
cd test; arm-linux-gnueabi-gcc -I../include trial.c -o trial1; cd ..;
cd test; arm-linux-gnueabi-gcc -I../include trial.c -o trial2; cd ..;
sudo cp test/selector ../mnt_dir/root;
sudo cp test/trial1 ../mnt_dir/root;
sudo cp test/trial2 ../mnt_dir/root;

#!/bin/bash

sudo ./build-rpi3-arm64.sh;
sudo ./scripts/mkbootimg_rpi3.sh;
sudo mv boot.img modules.img ../tizen-image;
sudo umount ../tizen-image/rootfs.img ../mnt_dir;
sudo mount ../tizen-image/rootfs.img ../mnt_dir;
cd test; arm-linux-gnueabi-gcc -I../include selector.c -o selector; cd ..;
cd test; arm-linux-gnueabi-gcc -I../include trial.c -o trial; cd ..;
cd test; arm-linux-gnueabi-gcc -I../include rotd.c -o rotd; cd ..;
sudo cp test/selector ../mnt_dir/root;
sudo cp test/trial ../mnt_dir/root;
sudo cp test/rotd ../mnt_dir/root;
cd ../mnt_dir;
mount -o rw,remount ./dev/root ./
#sudo sh qemu.sh

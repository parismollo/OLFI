#!/bin/bash

# Change to the /ouichefs directory
cd ouichefs-6.5.7/

# Run make with the specified KERNELDIR
make KERNELDIR=../linux-6.5.7/

# Copy the ouichefs.ko file to the ../share directory
cp ouichefs.ko ../share

# Change to the /mkfs directory
cd mkfs/

# Run the mkfs.ouichefs command on test.img
./mkfs.ouichefs test.img

# Copy the test.img file to the ../../share directory
cp test.img ../../share

# Change to the ../../ directory
cd ../../

# Start Linux
./start_linux

# Insert the ouichefs.ko kernel module
insmod /share/ouichefs.ko

# Mount the test.img file to /mnt/ouichefs using the loop option
mount -o loop /share/test.img /mnt/ouichefs

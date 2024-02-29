#! /bin/sh
#
# mk-kernel-img.sh
#
# Script to generate kernel.img image for Raspberry Pi devices.
# This contains the Cheviot bootloader and IFS image file concatenated
# into a single kernel.img file.  The IFS image is a simple filesystem
# containing the kernel, init command and drivers to bootstrap the
# system.
# 
# This kernel.img should be placed on the FAT boot partition along with
# the Qualcomm start.elf and bootcode.bin files (dependent on Pi version).
#

KERNEL_IFS_DIR=kernel_ifs_dir

mkdir -p build/$KERNEL_IFS_DIR
mkdir -p build/$KERNEL_IFS_DIR/bin
mkdir -p build/$KERNEL_IFS_DIR/boot
mkdir -p build/$KERNEL_IFS_DIR/dev
mkdir -p build/$KERNEL_IFS_DIR/dev/sda
mkdir -p build/$KERNEL_IFS_DIR/dev/tty	
mkdir -p build/$KERNEL_IFS_DIR/home
mkdir -p build/$KERNEL_IFS_DIR/media/root
mkdir -p build/$KERNEL_IFS_DIR/root
mkdir -p build/$KERNEL_IFS_DIR/sbin
mkdir -p build/$KERNEL_IFS_DIR/etc

cp build/host/boot/sbin/kernel      build/$KERNEL_IFS_DIR/boot/
cp build/host/sbin/init             build/$KERNEL_IFS_DIR/sbin/
cp build/host/sbin/aux              build/$KERNEL_IFS_DIR/sbin/
#cp build/host/sbin/sdcard           build/$KERNEL_IFS_DIR/sbin/
#cp build/host/sbin/extfs            build/$KERNEL_IFS_DIR/sbin/
cp build/host/sbin/ifs              build/$KERNEL_IFS_DIR/sbin/
cp build/host/sbin/devfs            build/$KERNEL_IFS_DIR/sbin/
cp build/host/etc/startup.cfg       build/$KERNEL_IFS_DIR/etc/
cp build/host/etc/profile.ksh       build/$KERNEL_IFS_DIR/etc/
cp build/host/bin/ksh               build/$KERNEL_IFS_DIR/bin/
cp build/host/bin/ls                build/$KERNEL_IFS_DIR/bin/
cp build/host/bin/env               build/$KERNEL_IFS_DIR/bin/
cp build/host/bin/cat               build/$KERNEL_IFS_DIR/bin/
cp build/host/bin/tr                build/$KERNEL_IFS_DIR/bin/
	
arm-none-eabi-objcopy build/host/sbin/boot -O binary output/boot.img

./output/src/mkifs-build/mkifs build/boot_partition/kernel.img output/boot.img build/$KERNEL_IFS_DIR



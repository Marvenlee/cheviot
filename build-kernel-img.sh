#! /bin/sh

KERNEL_IFS_DIR=kernel_ifs_dir

mkdir -p build/$KERNEL_IFS_DIR
mkdir -p build/$KERNEL_IFS_DIR/bin
mkdir -p build/$KERNEL_IFS_DIR/boot
mkdir -p build/$KERNEL_IFS_DIR/dev
mkdir -p build/$KERNEL_IFS_DIR/dev/sd1
mkdir -p build/$KERNEL_IFS_DIR/dev/tty	
mkdir -p build/$KERNEL_IFS_DIR/home
mkdir -p build/$KERNEL_IFS_DIR/media/sd1fs
mkdir -p build/$KERNEL_IFS_DIR/root
mkdir -p build/$KERNEL_IFS_DIR/sbin
mkdir -p build/$KERNEL_IFS_DIR/etc

cp build/host/boot/sbin/kernel      build/$KERNEL_IFS_DIR/boot/
cp build/host/sbin/init             build/$KERNEL_IFS_DIR/sbin/
cp build/host/sbin/serial           build/$KERNEL_IFS_DIR/sbin/
cp build/host/sbin/sdcard           build/$KERNEL_IFS_DIR/sbin/
cp build/host/sbin/fatfs            build/$KERNEL_IFS_DIR/sbin/
cp build/host/sbin/ifs              build/$KERNEL_IFS_DIR/sbin/
cp build/host/sbin/devfs            build/$KERNEL_IFS_DIR/sbin/
cp build/host/etc/startup.cfg       build/$KERNEL_IFS_DIR/etc/
cp build/host/bin/ksh               build/$KERNEL_IFS_DIR/bin/
cp build/host/bin/ls                build/$KERNEL_IFS_DIR/bin/
cp build/host/bin/env               build/$KERNEL_IFS_DIR/bin/
cp build/host/bin/cat               build/$KERNEL_IFS_DIR/bin/
cp build/host/bin/tr                build/$KERNEL_IFS_DIR/bin/
	
arm-none-eabi-objcopy build/host/sbin/boot -O binary output/boot.img

./output/src/mkifs-build/mkifs build/boot_partition/kernel.img output/boot.img build/$KERNEL_IFS_DIR



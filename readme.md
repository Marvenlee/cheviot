
# Cheviot Microkernel

## Licensing

Cheviot Microkernel (c) 2014 Marven Gilhespie

The Microkernel is under the Apache license, see source code for details.

The sdcard driver makes use of code by John Cronnin to access the SD card
and bootstrap the kernel.  It has it's own copyright.

The safe-string copying functions, Strlcat and Strlcpy are the
copyright of Todd C. Miller.


## Build instructions

CMake needs to be installed to build the project. Once installed enter the following
commands to build the GCC Cross Compiler, Newlib library and the kernel along with
several basic drivers, ksh shell and coreutils.

    $ mkdir build
    $ cd build
    $ source ../setup-env-sh
    $ cmake ..
    $ make

## Build the kernel.img 

Build the kernel.img with an embedded Image File System (IFS). The IFS is a simple
filesystem used to bootstrap the system and contains the kernel, init, IFS handler
and device drivers needed at startup.

From the build directory run the build-kernel-img.sh script to build the kernel.img
file in the build/boot_partition directory. This also places the required bootcode.bin
and start.elf files inside the same directory.  Copy all 3 files to the FAT boot
partition on the Raspberry Pi model A.

    $ ../build-kernel-img.sh




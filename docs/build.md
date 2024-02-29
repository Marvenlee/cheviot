# Building the Cheviot OS

# Requirements

A Linux system with common developer tools, gcc, binutils, cmake and sqlite3.
The following should get most of the required tools.

```
sudo apt install build-essential
sudo apt install cmake
sudo apt install sqlite3
sudo apt install sqlite3-dev
```

## Building the project

CMake is used as a meta-build tool that is used to build individual projects
that are typical built with Autotools/Automake.

Build the project with the following commands from within the directory this
readme.md resides in. This will build the GCC cross compiler, Newlib library
and the kernel along with ksh, coreutils and several basic drivers.

Modify the setup-env.sh to set the appropriate board, currently this
defaults to raspberrypi4.


```
    $ cd cheviot
    $ mkdir build
    $ cd build
    $ source ../setup-env.sh
    $ cmake ..
    $ make pseudo-native    
    $ make
```

This will build the projects and populate the build/host folder which contains
the directory tree of the final root partition.

We then run make image to build the final sd-card image:

```
    $ make image
```

This builds 3 images, the boot partition, the root partition and the combined sd-card image.

  * part1\_boot.fat
  * part2\_root.ext2
  * sdimage.img

The partitions can be mounted on a Linux system with the loopback device.  The sdimage.img
can be flashed to an SD-Card using dd, bmaptool or Balenaetcher.

# Notes

Pseudo is used so that commands can run as a "virtual" root user. Sqlite3 is used as a
database of the generated files "virtual" permissions so that we can set most files
as being owned by root and with appropriate permissions.

The 'make image' command runs the tools/mk-kernel.img.sh script to create the kernel.img
that is placed on the boot partition.  This combines the bootloader and Initial File System
(IFS) image which contains the required files to bootstrap the system. The required files
includes the kernel, UART and other drivers.


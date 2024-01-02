# Building the Cheviot OS

# Requirements

The following may be required to build

sudo apt install build-essential
sudo apt install cmake

Two precompiled binary blobs are required for the Raspberry Pi. These are
the start.elf and bootcode.bin files that are available in other Linux
repositories.  These need to be copied across to a third_party/blobs directory.


## Building the project

CMake is used as a meta-build tool that is used to build individual projects
that are typical built with Autotools/Automake.

Build the project with the following commands from within the directory this
readme.md resides in. This will build the GCC cross compiler, Newlib library
and the kernel along with ksh, coreutils and several basic drivers.

Modify the setup-env.sh to set the appropriate board, currently this
defaults to raspberrypi1 as support for other boards are a work in progress.

```
    $ cd cheviot
    $ mkdir build
    $ cd build
    $ source ../setup-env.sh
    $ cmake ..
    $ make
```

## Creating the kernel.img 

The above steps build the separate executables but we now need to combine these
into a single kernel.img that we can place on the Rasberry Pi's FAT boot partition.
This kernel.img combines a bootloader and a simple file system image containing
the kernel, SD Card and terminal drivers, init process and shell. This filesystem is the 
Initial File System (IFS).

From the build directory run the build-kernel-img.sh script to build the kernel.img
file in the build/boot_partition directory. This also places the required bootcode.bin
and start.elf files inside the same directory.  Copy all 3 files to the FAT boot
partition on the Raspberry Pi model A.

```
    $ ../tools/mk-kernel-img.sh
```

Connect a USB to FTDI serial adaptor from a PC to the Raspberry Pi GPIO header for
the serial port pins.  Start minicom and connect to the serial port.  Boot the Raspberry
Pi with the firmware on the SD card.  The Pi should boot and show some messages.
After a short pause it will reach a command prompt.


## Creating a SD-Card image for flashing with dd, Balena Etcher or bmaptool

From the build directory run the following script:

```
    $ ../tools/mk-sdcard-image.sh    
```

This is a work-in-progress.  Files in the image are assigned the current user's
UID and GID and not the root's UID and GID, 0 and 0. This will be fixed using the
"pseudo" tool.



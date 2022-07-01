
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
    $ source ../setup-env.sh
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


Connect a USB to FTDI serial adaptor from a PC to the Raspberry Pi GPIO header for
the serial port pins.  Start putty and connect to the serial port.  Boot the Raspberry
Pi with the firmware on the SD card.  The Pi should boot and show some messages.
After a short pause it will reach a command prompt.


# Introduction

Cheviot is a multi-server microkernel operating system for the original Raspberry Pi.
The kernel is quite compared to other microkernels as it deals with process management,
memory management and the virtual file system switch which includes file and pathname caching.

There are ports of:
  * Newlib which is used as the standard C library.
  * GNU Coreutils - common Unix commands such as ls, cat, sort, tr and tail
  * PD-KSH - The Korn shell

Drivers have been created for:
  * ifs - Init File System, a simple file system image for bootstrapping the OS.
  * serial - Serial port driver that appears as a tty.
  * devfs - Simple file system mounted as /dev
  * sdcard - Port of John Cronnin SD Card driver code
  * fatfs - FAT file system driver.


# Interprocess Communication and Drivers

Client applications do not send messages directly to file systems or drivers.
Instead traditional system calls such as open, read and write are first processed
by the microkernel.  If a file is cached within the kernel the these calls complete
without any IPC to the file system or drivers.  If the system call must fetch
data from the device it converts it to a message which is sent to the server.

A device driver or file system handler both act as servers.  They register a
message port with the kernel's VFS using the mount() system call.  This returns
a handle where it can receive messages from the VFS.

The driver does a receivemsg() to receive the command header from the file system.
This tells it what the command is and what parameters there are.

Additional readmsg() and writemsg() system calls may follow to read or write
the bulk of a message that follows. Once a message is handled the driver calls
replymsg() to notify the kernel that the command is complete.

See the system/drivers folder for examples of how drivers use this message passing API.

  
# Current Status

Cheviot is a long way from being a complete OS.  In it's current state it boots
into a KSH shell with the IFS image mounted as root.  This allows some basic
coreutils commands to be used to test out the system calls and C library.

# Interesting things

  * Build system uses cmake as a meta-make to import projects from elsewhere.
  * Patches to build Newlib, Coreutils and changes to Binutils.
  * Simple Init File System image format merged with kernel
  * Newlib changes to allow porting of Coreutils and PD-KSH.
  

# Future

The intial work that needs to be done is:

  * Finish of file system syscalls.
  * File creation and write handling, lazy flushing of cache.
  * SD Card driver with write support and determination of media size.
  * Ext File System support (avoids limits of the FAT FS).
  * Signal handling (API exists but handlers are not called)
  * Stride scheduler fixes.
  * Permission checks.
  * Replace IPC syscalls, receivemsg, replymsg, readmsg and writemsg with listen, read and write.
  
Further work:

  * Fine grained locking, replace big kernel lock with finer grained mutex/condition variables.
  * POSIX threads, currrently supports libtask coroutines.
  * Convert Memory segments back to linked lists.
  * Cleanup poll handling, add select.
  * Additional drivers for GPIO pins.
  * Self host GCC and binutils.
  * Port of a text editor
  

# Far Future

This project is a way of experimenting with operating systems and microkernel
design.  It would be interesting to split the kernel up into layers. One of the
original goals was to have a minimal microkernel acting as a trampoline. This
would implement only mechanisms to transfer control from one address space to
another through thread migration.

Much like the Linux 4G4G patch and Meltdown, the microkernel would only implement
mechanisms to do system call and return, interrupt and return and trap and return
to another address space.  This would allow implementing of several protection
rings. The process manager at ring 0, the VFS at ring 1, apps at ring 2 and
sandbox within an app in ring 3.





  
  









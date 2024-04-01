# Cheviot Microkernel

## Introduction

Cheviot is a multi-server microkernel operating system for the original Raspberry Pi.
The kernel's API is over 60 system calls and growing. These implement a POSIX-like
API.  This is different to many microkernels that tend to reduce the microkernel
to a minimal set of functionality. The kernel implements process management,
memory management and a virtual file system (VFS).

Within the kernel the VFS implements common POSIX-like system calls such as open, close,
read and write. These file system operations then send messages to processes that
handle the specifics of a file system format which in turn send messages to block device
drivers to read and write blocks from the physical media.

As message passing and context switching between processes has a not so insignificant overhead
the VFS implements a file level cache and directory lookup cache within the kernel. If a file
or portion of a file had been read previously and it's contents are still in the cache this will
eliminate a number of messages to file system handler and device driver processes.

This project is still in an early-alpha stage, as such many things remain to be implemented.


There are ports of third-party tools such as:
  * Newlib which is used as the standard C library.
  * GNU Coreutils - common Unix commands such as ls, cat, sort, tr and tail
  * PD-KSH - The Korn shell

Drivers have been created for:
  * serial - Serial port driver that appears as a tty.
  * sdcard - Port of John Cronnin SD Card driver code

Filesystems have been created for:
  * ifs - Initial File System, a simple file system image for bootstrapping the OS.
  * devfs - Simple file system mounted as /dev
  * extfs - partially implemented (based on Minix extfs driver)
  * fatfs - partially implemented (requires rewrite).


## Documentation

[ipc.md](docs/ipc.md) - Notes on message passing and servers
[build.md](docs/build.md) - Build instructions
[status.md](docs/status.md) - Current project status and plans


## Licensing

The microkernel itself is under the Apache license, see source code for details.

The Qualcomm binary blobs start.elf and bootcode.bin needed by the Raspberry Pi
in order to boot are located in third\_party/blobs.  These have their own license
in [third_party/blobs/LICENSE.broadcom](third_party/blobs/LICENSE.broadcom).

The sdcard driver makes use of code by John Cronnin to access the SD card
and bootstrap the kernel.  It has it's own copyright.

The safe-string copying functions, Strlcat and Strlcpy are the
copyright of Todd C. Miller.


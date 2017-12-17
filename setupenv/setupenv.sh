#!/bin/sh

# ***************************************************************************
#
# TODO:  Modify these build scripts taken from my old Kielder OS project
# and use them as a basis for setting up the build environment for Cheviot.
#
# ***************************************************************************







# ***************************************************************************
# GNU Binutils
#
# Builds and installs an i386-kielder Binutils on the current build system.
#
# The major change is the addition of the file
# "binutils-2.17/ld/emulparams/elf_i386_kielder.sh"
# 
# The lines "TEXT_START_ADDR=0xA0000000" and
# "NONPAGED_TEXT_START_ADDR=0xA0000000" sets the default start address to
# 0xA0000000 as the Kielder kernel resides in the lower half of the address
# space.
#

tar jxf binutils-2.17.tar.bz2
mkdir build_binutils -p
cd build_binutils

../binutils-2.17/configure --target=arm-none-eabi --prefix= $HOME/build_tools -v
make all install

cd ..


# ***************************************************************************
# Append the cross-compiler bin directory to the current
# search path. 
#

PATH="/kielder/bin:$PATH"


# Check if .bashrc exists, append to bottom.  Is there a cleaner way?


# ***************************************************************************
# Uncomment out the following line to permanently add the "/kielder/bin"
# directory to the search path.  Or manually alter "/etc/profile" later.
#

# printf '\nPATH="$HOME/build_tools/bin:$PATH"\nexport PATH\n' >>$HOME/.profile




# ***************************************************************************
# GNU GCC (running on Cygwin)
#
# Builds and installs an i386-kielder GCC cross-compiler on the current
# build system.
#
# Major change is the addition of "gcc-4.1.1/gcc/config/i386/kielder.h"
# This file specifies what the "crt" files are named and what default
# libraries are linked when creating executables with i386-kielder-gcc.

tar jxf gcc-core-4.1.1.tar.bz2
patch -p0 <gcc.patch

mkdir build_gcc
cd build_gcc

../gcc-4.1.1/configure \
--target=i386-kielder \
--with-newlib \
--with-included-gettext \
--enable-languages=c \
--disable-libssp \
--prefix=/kielder -v

make all install

cd ..




# ***************************************************************************
# Newlib (for i386-kielder target)
#
# Builds and installs Newlib 1.15 for the i386-kielder target.
# libraries are placed in /kielder/i386-kielder/lib
# includes are placed in /kielder/i386-kielder/include
#
# Added "kielder" directory that contains the glue code between Newlib and
# the operating system as well as features that Newlib doesn't provide.
# This is located at "newlib-1.15.0_new\newlib\libc\sys\kielder"
#
# crt0.o at this moment doesn't handle constructors or destructors.
#
#
# A major configuration point is in "newlib-1.14.0/newlib/configure.host"
# Search for "kielder" in the above file.  Various configuration switches
# are set in the line starting with "newlib_cflags="${newlib_cflags}"
# 
# For Kielder, the following is used...
#
# -DPREFER_SIZE_OVER_SPEED -D_I386MACH_ALLOW_HW_INTERRUPTS
# -DHAVE_GETTIMEOFDAY -DMALLOC_PROVIDED -DEXIT_PROVIDED
# -DMISSING_SYSCALL_NAMES -DSIGNAL_PROVIDED -DHAVE_OPENDIR
# -DHAVE_FCNTL -DHAVE_RENAME
#
# When files are changed, added or removed from the
# "newlib-1.15.0/newlib/libc/sys/kielder" directory then add or remove
# the filename from "makefile.am" and run the following
# commands from the directory to update the makefiles.
#
# aclocal -I ../../..
# autoconf
# automake --cygnus Makefile
#

tar zxf newlib-1.15.0.tar.gz
patch -p0 <newlib.patch

mkdir build_newlib
cd build_newlib

../newlib-1.15.0/configure --target=i386-kielder --prefix=/kielder

make all install

cd ..




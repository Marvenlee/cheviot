# Usage
#
# make                - make everything, kernel, init, format, assign ...
# make depend         - update all dependencies
# make clean          - remove all 
# make disk           - copy all files to disk image with mtools
#                       (ensure directories c:/bin, c:/etc c:/home exist)
#
# Can also update specific parts like this...
# 
# make kernel         - make the kernel
# make kernel/depend  - update kernel dependencies
# make kernel/clean   - remove kernel exe and object files
# make kernel/disk    - Copy kernel to disk image with mtools
#
#
# FIXME: Makedepend uses the default compiler include path when
# including stdarg.h, which is different to the cheviot-elf cross compiler one.
# May need to modify Makedepend source and recompile to use a compiler
# other than the default GCC.
#
# ----------------------------------------------------------------------------

LINK:= arm-none-eabi-ld
CC:= arm-none-eabi-gcc
CPP:= arm-none-eabi-g++
AS:=arm-none-eabi-as
STRIP:= arm-none-eabi-strip
OBJCOPY:= arm-none-eabi-objcopy
OBJDUMP:= arm-none-eabi-objdump
ARCHIVE:= arm-none-eabi-ar
OFLAGS:=
AFLAGS:= -r -I./h 
CFLAGS:= -Wall -I./h -O2 -std=c99
LFLAGS:= -v --library-path="/home/marven/build_tools/lib/gcc/arm-none-eabi/4.9.2/fpu"

DFLAGS:= -I/cheviot/arm-none-eabi/include -I./kernel/h \
			-I/cheviot/lib/gcc/arm-none-eabi/4.9.2/include
DEPEND:= makedepend 
APPLINK:= arm-none-eabi-gcc
APPLDFLAGS:= 
INSTALLDIR:=/cheviot/arm-none-eabi/


# Could add -fno-common to CFLAGS



# ----------------------------------------------------------------------------

.PHONY : everything
.PHONY : all
.PHONY : depend
.PHONY : clean
.PHONY : dump
.PHONY : strip
.PHONY : install


# ----------------------------------------------------------------------------
# default rule when calling make,  placed before any makefiles are included.

everything : all

# ----------------------------------------------------------------------------
# Include subdirectory makefiles here

include kernel/makefile.in
include filesys/makefile.in
include boot/makefile.in

# ----------------------------------------------------------------------------
# Generic pattern rules.
# More specialized rules may be defined in the included makefiles above
# and will have precedence over these generic rules. The specialized rules
# may choose to have different include directories for example

%.o : %.cc
	$(CPP) -c $(CFLAGS) $< -o $@

%.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@

%.o : %.s
	$(AS) $(AFLAGS) $< -o $@

%.o : %.S
	$(CC) -c $(CFLAGS) $< -o $@



# ----------------------------------------------------------------------------
# add targets for "make all" here.
        
all:	kernel \
		filesys \
		boot


# ----------------------------------------------------------------------------

clean: 	kernel/clean \
		filesys/clean \
		boot/clean
	
	
# ----------------------------------------------------------------------------

depend: kernel/depend \
		filesys/depend \
		boot/depend 


# ----------------------------------------------------------------------------

strip: 	kernel/strip \
		filesys/strip \
		boot/strip
		

# ----------------------------------------------------------------------------

# DO NOT DELETE







sbin_PROGRAMS = kernel

kernel_SOURCES =

include arm/Makefile.am
include proc/Makefile.am
include fs/Makefile.am
include utility/Makefile.am
include vm/Makefile.am


AM_CFLAGS = -O2 -std=c99 -g0
AM_CPPFLAGS = -I$(srcdir)/h
AM_CCASFLAGS = -r -I$(srcdir)/h

kernel_LDFLAGS = -v \
                 -nostartfiles \
                 -lgcc \
                 -T $(srcdir)/arm/linker.ls


#LFLAGS:= -v --library-path="/home/marven/build_tools/lib/gcc/arm-none-eabi/4.9.2/fpu"
#
#DFLAGS:= -I/cheviot/arm-none-eabi/include  \
#			-I/cheviot/lib/gcc/arm-none-eabi/4.9.2/include


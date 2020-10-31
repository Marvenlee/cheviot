
sbin_PROGRAMS = boot

boot_SOURCES = \
  arm.S \
  bootstrap.c \
  debug.c \
  dbgfont.c \
  doprintf.c \
  elf.c \
  globals.c \
  main.c \
  mbox.c \
  memory.c \
  mmio.c \
  start.S
  

AM_CFLAGS = -O2 -std=c99 -g0
AM_CPPFLAGS = -I../kernel/h
AM_CCASFLAGS = -r -I../kernel/h

boot_LDFLAGS = -v \
                 -nostartfiles \
                 -lgcc \
                 -T $(srcdir)/linker.ls
                 
# TODO: post step to convert to binary, unless we use linker flag to output binary
                 

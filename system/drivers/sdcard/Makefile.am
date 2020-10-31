
sbin_PROGRAMS = sdcard

sdcard_SOURCES = \
  emmc.c \
  globals.c \
  main.c \
  mbox.c \
  mmio.c \
  timer.c 

AM_CFLAGS = -O2 -std=c99 -g0
AM_CPPFLAGS = -I$(top_srcdir)/kernel/h
AM_CCASFLAGS = -r -I$(top_srcdir)/kernel/h

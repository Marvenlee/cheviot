
bin_PROGRAMS = hexdump

hexdump_SOURCES = \
  conv.c \
  display.c \
	hexdump.c \
  hexsyntax.c \
  odsyntax.c \
  parse.c
  
hexdump_LDADD = -lutil

AM_CFLAGS = -O0 -std=c99 -g2
AM_CPPFLAGS = -I$(srcdir)

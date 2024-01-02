lib_LIBRARIES = libcurses.a

libcurses_a_SOURCES = \
  curses.c

include_HEADERS = curses.h

AM_CFLAGS = -O2 -std=c99 -g0
AM_CPPFLAGS = -I. -I$(srcdir)/../libtermcap
AM_CCASFLAGS = -r -I. -I$(srcdir)/../libtermcap 

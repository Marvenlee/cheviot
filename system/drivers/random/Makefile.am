sbin_PROGRAMS = random


if BOARD_RASPBERRY_PI_4

random_SOURCES = \
  globals.c \
  init.c \
  main.c
  
random_LDADD = -lrpihal
AM_CFLAGS = -O0 -std=c99 -g2

random_SOURCES += boards/raspberry_pi_4/trng.c

AM_CPPFLAGS = -I$(srcdir) -I$(srcdir)/boards/raspberry_pi_4 -DBOARD_RASPBERRY_PI_4
AM_CCASFLAGS = -r -I$(srcdir) -I$(srcdir)/boards/raspberry_pi_4 -DBOARD_RASPBERRY_PI_4

endif





sbin_PROGRAMS = sdcard

sdcard_SOURCES = \
  emmc.c \
  globals.c \
  init.c \
  main.c \
  mmio.c \
  timer.c 

sdcard_LDADD = -lrpimailbox -lrpigpio -lrpihal

if BOARD_RASPBERRY_PI_1
AM_CFLAGS = -O2 -std=c99 -g0
AM_CPPFLAGS = -I$(srcdir)/boards/raspberry_pi_1 -DBOARD_RASPBERRY_PI_1
AM_CCASFLAGS = -r -I$(srcdir)/boards/raspberry_pi_1 -DBOARD_RASPBERRY_PI_1
endif

if BOARD_RASPBERRY_PI_4
AM_CFLAGS = -O2 -std=c99 -g0
AM_CPPFLAGS = -I$(srcdir)/boards/raspberry_pi_4 -DBOARD_RASPBERRY_PI_4
AM_CCASFLAGS = -r -I$(srcdir)/boards/raspberry_pi_4 -DBOARD_RASPBERRY_PI_4
endif


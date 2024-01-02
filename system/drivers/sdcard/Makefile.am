
sbin_PROGRAMS = sdcard

sdcard_SOURCES = \
  emmc.c \
  globals.c \
  init.c \
  main.c \
  mbox.c \
  mmio.c \
  timer.c 

if BOARD_RASPBERRY_PI_1

AM_CFLAGS = -O2 -std=c99 -g0
AM_CPPFLAGS = -I$(srcdir)/boards/raspberry_pi_1 -DBOARD_RASPBERRY_PI_1
AM_CCASFLAGS = -r -I$(srcdir)/boards/raspberry_pi_1 -DBOARD_RASPBERRY_PI_1

else
if BOARD_RASPBERRY_PI_4

AM_CFLAGS = -O2 -std=c99 -g0
AM_CPPFLAGS = -I$(srcdir)/boards/raspberry_pi_4 -DBOARD_RASPBERRY_PI_4
AM_CCASFLAGS = -r -I$(srcdir)/boards/raspberry_pi_4 -DBOARD_RASPBERRY_PI_4

else
  echo "BOARD env variable not valid";
  exit -1;

endif
endif


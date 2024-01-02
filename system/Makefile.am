
if BOARD_RASPBERRY_PI_1

AM_CFLAGS = -O2 -std=c99 -g0
AM_CPPFLAGS = -DBOARD_RASPBERRY_PI_1
AM_CCASFLAGS = -r -DBOARD_RASPBERRY_PI_1

else
if BOARD_RASPBERRY_PI_4

AM_CFLAGS = -O2 -std=c99 -g0
AM_CPPFLAGS = -DBOARD_RASPBERRY_PI_4
AM_CCASFLAGS = -r -DBOARD_RASPBERRY_PI_4

else
  echo "BOARD environment variable not valid";
  exit -1;

endif
endif


SUBDIRS = boot drivers filesystems init etc

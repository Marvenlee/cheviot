
if BOARD_RASPBERRY_PI_1

AM_CFLAGS = -O0 -std=c99 -g2 -march=armv7ve
AM_CPPFLAGS = -DBOARD_RASPBERRY_PI_1
AM_CCASFLAGS = -r -DBOARD_RASPBERRY_PI_1 -march=armv7ve

else
if BOARD_RASPBERRY_PI_4

AM_CFLAGS = -O0 -std=c99 -g2 -march=armv7ve
AM_CPPFLAGS = -DBOARD_RASPBERRY_PI_4
AM_CCASFLAGS = -r -DBOARD_RASPBERRY_PI_4 -march=armv7ve

else
  echo "BOARD environment variable not valid";
  exit -1;

endif
endif

SUBDIRS = boot drivers filesystems init etc readfile



sbin_PROGRAMS = serial

serial_SOURCES = \
  arm.S \
  globals.c \
  init.c \
  main.c
  

serial_LDADD = -ltask


if BOARD_RASPBERRY_PI_1

serial_SOURCES += boards/raspberry_pi_1/init_pl011_uart.c \
                  boards/raspberry_pi_1/pl011_uart.c

AM_CFLAGS = -O2 -std=c99 -g0
AM_CPPFLAGS = -I$(srcdir) -I$(srcdir)/boards/raspberry_pi_1 -DBOARD_RASPBERRY_PI_1
AM_CCASFLAGS = -r -I$(srcdir) -I$(srcdir)/boards/raspberry_pi_1 -DBOARD_RASPBERRY_PI_1


else
if BOARD_RASPBERRY_PI_4

serial_SOURCES += boards/raspberry_pi_4/init_mini_uart.c \
                  boards/raspberry_pi_4/mini_uart.c

AM_CFLAGS = -O2 -std=c99 -g0
AM_CPPFLAGS = -I$(srcdir) -I$(srcdir)/boards/raspberry_pi_4 -DBOARD_RASPBERRY_PI_4
AM_CCASFLAGS = -r -I$(srcdir) -I$(srcdir)/boards/raspberry_pi_4 -DBOARD_RASPBERRY_PI_4

else
  echo "BOARD env variable not valid";
  exit -1;

endif
endif


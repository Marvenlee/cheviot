
sbin_PROGRAMS = serial

serial_SOURCES = \
  globals.c \
  init.c \
  main.c
  
serial_LDADD = -ltask -lrpihal

if BOARD_RASPBERRY_PI_1
serial_SOURCES += boards/raspberry_pi_1/pl011_uart.c \
                  boards/raspberry_pi_1/gpio.c
                  
AM_CFLAGS = -O2 -std=c99 -g0
AM_CPPFLAGS = -I$(srcdir) -I$(srcdir)/boards/raspberry_pi_1 -DBOARD_RASPBERRY_PI_1
AM_CCASFLAGS = -r -I$(srcdir) -I$(srcdir)/boards/raspberry_pi_1 -DBOARD_RASPBERRY_PI_1
endif

if BOARD_RASPBERRY_PI_4
serial_SOURCES += boards/raspberry_pi_4/pl011_uart.c \
                  boards/raspberry_pi_4/gpio.c

AM_CFLAGS = -O2 -std=c99 -g0
AM_CPPFLAGS = -I$(srcdir) -I$(srcdir)/boards/raspberry_pi_4 -DBOARD_RASPBERRY_PI_4
AM_CCASFLAGS = -r -I$(srcdir) -I$(srcdir)/boards/raspberry_pi_4 -DBOARD_RASPBERRY_PI_4
endif


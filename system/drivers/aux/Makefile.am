sbin_PROGRAMS = aux

aux_SOURCES = \
  globals.c \
  init.c \
  main.c
  
aux_LDADD = -ltask -lrpihal
AM_CFLAGS = -O0 -std=c99 -g2 -march=armv7ve

if BOARD_RASPBERRY_PI_1
aux_SOURCES += boards/raspberry_pi_1/aux_uart.c \
               boards/raspberry_pi_1/gpio.c \
               boards/raspberry_pi_1/init.c
      
AM_CPPFLAGS = -I$(srcdir) -I$(srcdir)/boards/raspberry_pi_1 -DBOARD_RASPBERRY_PI_1
AM_CCASFLAGS = -r -I$(srcdir) -I$(srcdir)/boards/raspberry_pi_1 -DBOARD_RASPBERRY_PI_1
endif

if BOARD_RASPBERRY_PI_4
aux_SOURCES += boards/raspberry_pi_4/aux_uart.c \
               boards/raspberry_pi_4/gpio.c \
               boards/raspberry_pi_4/init.c

AM_CPPFLAGS = -I$(srcdir) -I$(srcdir)/boards/raspberry_pi_4 -DBOARD_RASPBERRY_PI_4
AM_CCASFLAGS = -r -I$(srcdir) -I$(srcdir)/boards/raspberry_pi_4 -DBOARD_RASPBERRY_PI_4
endif




lib_LIBRARIES = librpigpio.a

librpigpio_a_SOURCES = \
	gpio.c \
  syscall.S
  
nobase_include_HEADERS = sys/rpi_gpio.h

AM_CFLAGS = -O2 -std=c99 -g0 -I.
AM_CCASFLAGS = -r -I.


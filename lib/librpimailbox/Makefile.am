lib_LIBRARIES = librpimailbox.a

librpimailbox_a_SOURCES = \
	mailbox.c \
  mailbox_clock.c \
  mailbox_gpio.c \
  mailbox_power.c \
  syscall.S
  
nobase_include_HEADERS = sys/rpi_mailbox.h

AM_CFLAGS = -O2 -std=c99 -g0 -I.
AM_CCASFLAGS = -r -I.


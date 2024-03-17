
sbin_PROGRAMS = boot

boot_SOURCES = \
  doprintf.c \
  elf.c \
  globals.c \
  main.c


if BOARD_RASPBERRY_PI_1

boot_SOURCES += boards/raspberry_pi_1/aux_uart.c \
                boards/raspberry_pi_1/bootstrap.c \
                boards/raspberry_pi_1/debug.c \
                boards/raspberry_pi_1/gpio.c \
                boards/raspberry_pi_1/init.c \
                boards/raspberry_pi_1/led.c \
                boards/raspberry_pi_1/memory.c \
                boards/raspberry_pi_1/start.S 

boot_LDADD = -lrpimailbox -lrpihal

AM_CFLAGS = -O2 -std=c99 -g0
AM_CPPFLAGS = -I$(srcdir)/boards/raspberry_pi_1 -DBOARD_RASPBERRY_PI_1
AM_CCASFLAGS = -r -I$(srcdir)/boards/raspberry_pi_1 -DBOARD_RASPBERRY_PI_1

boot_LDFLAGS = -v \
               -nostartfiles \
               -lgcc \
               -T $(srcdir)/boards/raspberry_pi_1/linker.ls
endif

if BOARD_RASPBERRY_PI_4

boot_SOURCES += boards/raspberry_pi_4/aux_uart.c \
                boards/raspberry_pi_4/bootstrap.c \
                boards/raspberry_pi_4/debug.c \
                boards/raspberry_pi_4/gpio.c \
                boards/raspberry_pi_4/init.c \
                boards/raspberry_pi_4/start.S 

boot_LDADD = -lrpimailbox -lrpihal

AM_CFLAGS = -march=armv7ve -O0 -std=c99 -g2
AM_CPPFLAGS = -I$(srcdir)/boards/raspberry_pi_4 -DBOARD_RASPBERRY_PI_4
AM_CCASFLAGS = -march=armv7ve -r -I$(srcdir)/boards/raspberry_pi_4 -DBOARD_RASPBERRY_PI_4

boot_LDFLAGS = -v \
               -nostartfiles \
               -lgcc \
               -T $(srcdir)/boards/raspberry_pi_4/linker.ls
endif



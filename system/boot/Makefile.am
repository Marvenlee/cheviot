
sbin_PROGRAMS = boot

boot_SOURCES = \
  bootstrap.c \
  doprintf.c \
  elf.c \
  globals.c \
  main.c \
  mbox.c \
  memory.c \
  mmio.c \
  start.S


if BOARD_RASPBERRY_PI_1

boot_SOURCES += boards/raspberry_pi_1/arm.S \
                boards/raspberry_pi_1/arm_misc.c \
                boards/raspberry_pi_1/debug_pl011_uart.c

AM_CFLAGS = -O2 -std=c99 -g0
AM_CPPFLAGS = -I$(srcdir)/boards/raspberry_pi_1 -DBOARD_RASPBERRY_PI_1
AM_CCASFLAGS = -r -I$(srcdir)/boards/raspberry_pi_1 -DBOARD_RASPBERRY_PI_1

boot_LDFLAGS = -v \
               -nostartfiles \
               -lgcc \
               -T $(srcdir)/linker.ls


else
if BOARD_RASPBERRY_PI_4

boot_SOURCES += boards/raspberry_pi_4/arm.S \
                boards/raspberry_pi_4/arm_misc.c \
                boards/raspberry_pi_4/debug_mini_uart.c


AM_CFLAGS = -O2 -std=c99 -g0
AM_CPPFLAGS = -I$(srcdir)/boards/raspberry_pi_4 -DBOARD_RASPBERRY_PI_4
AM_CCASFLAGS = -r -I$(srcdir)/boards/raspberry_pi_4 -DBOARD_RASPBERRY_PI_4

boot_LDFLAGS = -v \
               -nostartfiles \
               -lgcc \
               -T $(srcdir)/linker.ls

else
  echo "BOARD env variable not valid";
  exit -1;

endif
endif



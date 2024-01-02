# Raspberry Pi 1 HAL Library (for kernel, bootloader, serial and SD-Card drivers */

lib_LIBRARIES = librpihal.a

librpihal_a_SOURCES = \
  hal_aux.c \
  hal_gpio.c \
  hal_interrupt.c \
  hal_mailbox.c \
  hal_memory.c \
  hal_mmio.c \
  hal_pl011_uart.c \
  hal_timer.c \
  hal_videocore.c
  
#nobase_include_HEADERS = machine/boards/raspberry_pi/

AM_CFLAGS = -O2 -std=c99 -g0 -I.
AM_CCASFLAGS = -r -I.


# Raspberry Pi 1 HAL Library (for kernel, bootloader, serial and SD-Card drivers */

lib_LIBRARIES = librpihal.a

librpihal_a_SOURCES = \
  hal_arm.S \
  hal_mailbox.c \
  hal_mmio.c
    
nobase_include_HEADERS = machine/cheviot_hal.h \
                         machine/board/hal_arm.h \
                         machine/board/hal_mailbox.h \
                         machine/board/hal_mmio.h

AM_CFLAGS = -O2 -std=c99 -g0 -I.
AM_CCASFLAGS = -r -I.


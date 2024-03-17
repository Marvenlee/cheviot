#ifndef GLOBALS_H
#define GLOBALS_H

#include "sdcard.h"
#include <sys/syscalls.h>
#include <sys/types.h>



extern uint32_t mbox_base;
extern uint32_t emmc_base;

//extern uint32_t mailbuffer_virt_addr;
//extern uint32_t mailbuffer_phys_addr;

extern struct block_device actual_device;
extern struct block_device *bdev;

extern uint8_t bootsector[512];
extern uint8_t *buf;

extern int kq;

extern struct Config config;

extern int nunits;
extern struct bdev_unit unit[5];


/*
 *
 */
//extern uint32 mailbuffer[64];

//extern volatile uint32 *gpio;

#endif

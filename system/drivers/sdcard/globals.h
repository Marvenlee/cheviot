#ifndef GLOBALS_H
#define GLOBALS_H

#include "block.h"
#include <sys/syscalls.h>
#include <sys/types.h>

#define MB_ADDR_PA 0x40007000
#define EMMC_BASE_PA 0x20300000
#define TIMER_BASE_PA 0x20003000
#define TIMER_CLO_OFFSET 4
#define MBOX_BASE_PA 0x2000b000
#define MBOX_BASE_OFFSET 0x00000880

extern uint32_t timer_clo;
extern uint32_t mbox_base;
extern uint32_t mb_addr;
extern uint32_t emmc_base;

extern struct block_device actual_device;
extern struct block_device *bdev;

extern struct FatSB fsb;
// extern struct FatFilp filp;
// extern struct FatNode node;

// extern int32 current_fat_sector;
// extern uint32 lseek_fat_entry;

extern uint8 bootsector[512];
// extern uint8 bpb_sector[512];

int fat_buf_sector;
extern uint8 fat_buf[512];

int file_buf_sector;
extern uint8 file_buf[512];

extern uint32 partition_start;



extern uint8_t *buf;
extern int fd;
extern int connection_socket;

extern struct Config config;

/*
 *
 */

extern uint32 mailbuffer[64];

/*
extern uint32 arm_mem_base;
extern uint32 arm_mem_size;
extern uint32 vc_mem_base;
extern uint32 vc_mem_size;
*/

/* Debugger stuff */

extern volatile uint32 *gpio;

/*
extern uint32 screen_width;
extern uint32 screen_height;
extern uint32 screen_buf;
extern uint32 screen_pitch;

extern bool __debug_enabled;
*/

#endif

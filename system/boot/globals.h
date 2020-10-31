#ifndef GLOBALS_H
#define GLOBALS_H

#include "bootinfo.h"
#include "elf.h"
#include "memory.h"
#include "types.h"

extern struct BootInfo bootinfo;

extern vm_addr user_base;

extern Elf32_EHdr ehdr;
extern Elf32_PHdr phdr_table[MAX_PHDR];
extern int phdr_cnt;

/*
 *
 */

extern uint32 mailbuffer[64];
extern uint32 arm_mem_base;
extern uint32 arm_mem_size;
extern uint32 vc_mem_base;
extern uint32 vc_mem_size;

/* Debugger stuff */

extern volatile struct bcm2835_gpio_registers *gpio_regs;
extern uint32 screen_width;
extern uint32 screen_height;
extern uint32 screen_pitch;
extern void *screen_buf;

// extern bool __debug_enabled;

#endif

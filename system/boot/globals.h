#ifndef GLOBALS_H
#define GLOBALS_H

#include "bootinfo.h"
#include "elf.h"
#include "memory.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <machine/cheviot_hal.h>

extern struct BootInfo bootinfo;

extern vm_addr user_base;

extern Elf32_EHdr ehdr;
extern Elf32_PHdr phdr_table[MAX_PHDR];
extern int phdr_cnt;

/*
 *
 */

extern uint32_t mailbuffer[64];
extern uint32_t arm_mem_base;
extern uint32_t arm_mem_size;
extern uint32_t vc_mem_base;
extern uint32_t vc_mem_size;

/* Debugger stuff */

extern uint32_t screen_width;
extern uint32_t screen_height;
extern uint32_t screen_pitch;
extern void *screen_buf;

// extern bool __debug_enabled;

#endif

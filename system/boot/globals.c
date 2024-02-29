#include "globals.h"
#include "bootinfo.h"
#include "elf.h"
#include "memory.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <machine/cheviot_hal.h>

struct BootInfo bootinfo;

vm_addr user_base;

Elf32_EHdr ehdr;
Elf32_PHdr phdr_table[MAX_PHDR];
int phdr_cnt;

/*
 *
 */

uint32_t arm_mem_base;
uint32_t arm_mem_size;
uint32_t vc_mem_base;
uint32_t vc_mem_size;

/* Debugger stuff */

uint32_t screen_width;
uint32_t screen_height;
uint32_t screen_pitch;
void *screen_buf;

// bool __debug_enabled;

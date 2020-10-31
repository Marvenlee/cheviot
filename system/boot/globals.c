#include "globals.h"
#include "bootinfo.h"
#include "elf.h"
#include "memory.h"
#include "types.h"

struct BootInfo bootinfo;

vm_addr user_base;

Elf32_EHdr ehdr;
Elf32_PHdr phdr_table[MAX_PHDR];
int phdr_cnt;

/*
 *
 */

uint32 arm_mem_base;
uint32 arm_mem_size;
uint32 vc_mem_base;
uint32 vc_mem_size;

/* Debugger stuff */

volatile struct bcm2835_gpio_registers *gpio_regs;
uint32 screen_width;
uint32 screen_height;
uint32 screen_pitch;
void *screen_buf;

// bool __debug_enabled;

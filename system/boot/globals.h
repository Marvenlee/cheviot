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


#endif

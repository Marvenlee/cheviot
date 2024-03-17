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



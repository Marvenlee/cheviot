#ifndef BOOTINFO_H
#define BOOTINFO_H

#include "elf.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "machine/cheviot_hal.h"

#define MAX_BOOTINFO_CPU 			4
#define MAX_BOOTINFO_PHDRS    16
#define CPUSTRSZ 	16

struct BootInfo {
  void *root_pagedir;
  void *kernel_pagetables;
  void *bootloader_pagetables;

	uint32_t revision;
	uint32_t board_type;
	uint32_t cpu_type;

  char cpustr[CPUSTRSZ];
  vm_size mem_size;
  vm_addr videocore_base;
  vm_addr videocore_ceiling;
  
  vm_addr bootloader_base;
  vm_addr bootloader_ceiling;

  vm_addr kernel_base;
  vm_addr kernel_ceiling;

  vm_addr pagetable_base;
  vm_addr pagetable_ceiling;

	vm_addr io_base;
	vm_addr io_ceiling;
	
	vm_addr timer_base;
	vm_addr gpio_base;
	vm_addr aux_base;
	vm_addr gicd_base;
	vm_addr gicc_base[MAX_BOOTINFO_CPU];
	vm_addr legacy_armc_base;
  vm_addr arm_base;						// CPU-specific registers
	vm_addr mailbox_base;
	
  int kernel_phdr_cnt;

  Elf32_EHdr kernel_ehdr;
  Elf32_PHdr kernel_phdr[MAX_BOOTINFO_PHDRS];
  
  vm_addr ifs_image;
  vm_size ifs_image_size;
  
  vm_addr ifs_exe_base;
  vm_size ifs_exe_size;
};



#endif

#ifndef KERNEL_ARM_BOOT_H
#define KERNEL_ARM_BOOT_H

#include <kernel/arm/arm.h>
#include <kernel/types.h>
#include <kernel/elf.h>


#define MAX_BOOTINFO_PHDRS    16

struct BootInfo {
  void *root_pagedir;
  void *kernel_pagetables;
  void *root_pagetables;

  uint32_t screen_width;
  uint32_t screen_height;
  uint32_t screen_pitch;
  void *screen_buf;
  vm_size mem_size;

  vm_addr bootloader_base;
  vm_addr bootloader_ceiling;

  vm_addr kernel_base;
  vm_addr kernel_ceiling;

  vm_addr pagetable_base;
  vm_addr pagetable_ceiling;

  int kernel_phdr_cnt;

  Elf32_EHdr kernel_ehdr;
  Elf32_PHdr kernel_phdr[MAX_BOOTINFO_PHDRS];
    
  vm_addr ifs_image;
  vm_size ifs_image_size;
  
  vm_addr ifs_exe_base;   // Location within the ifs image
  vm_size ifs_exe_size;
};


/*
 * Prototypes
 */
void SwitchToRoot(vm_addr sp);

#endif

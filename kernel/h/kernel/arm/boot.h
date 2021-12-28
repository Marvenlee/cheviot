#ifndef KERNEL_ARM_BOOT_H
#define KERNEL_ARM_BOOT_H

#include <kernel/arm/arm.h>
#include <kernel/types.h>

struct BootInfo {
  void *root_pagedir;
  void *kernel_pagetables;

  // What is root_pagetables, is it the bootloader?  What about IFS.exe?  
  void *root_pagetables;
  
  // How do we know where root_pagetables is or the amount mof memory the bootloader
  // is using?
  
  
  void (*bootloader_callback)(void);  // Return point back into bootloader
  
  uint32_t screen_width;
  uint32_t screen_height;
  uint32_t screen_pitch;
  void *screen_buf;
  vm_size mem_size;
  
  vm_addr pagetable_base;
  vm_addr pagetable_ceiling;
};

// Prototypes
void SwitchToRoot(vm_addr sp);

#endif

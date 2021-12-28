#ifndef BOOTINFO_H
#define BOOTINFO_H

#include "types.h"
#include <stdint.h>

struct BootInfo {
  void *root_pagedir;
  void *kernel_pagetables;
  void *root_pagetables;

  void (*bootloader_callback)(void);  // Return point back into bootloader
  
  uint32_t screen_width;
  uint32_t screen_height;
  uint32_t screen_pitch;
  void *screen_buf;
  vm_size mem_size;

  vm_addr pagetable_base;
  vm_addr pagetable_ceiling;
};

#endif

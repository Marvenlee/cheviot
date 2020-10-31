#ifndef KERNEL_ARM_BOOT_H
#define KERNEL_ARM_BOOT_H

#include <kernel/arm/arm.h>
#include <kernel/types.h>

struct BootInfo {
  void *root_pagedir;
  void *kernel_pagetables;
  void *root_pagetables;
  uint32_t screen_width;
  uint32_t screen_height;
  uint32_t screen_pitch;
  void *screen_buf;
  vm_size mem_size;
  void *ifs_image;
  uint32_t ifs_image_size;
  vm_addr pagetable_base;
  vm_addr pagetable_ceiling;
};

// Prototypes
void SwitchToRoot(vm_addr sp);

#endif

/*
 * Copyright 2014  Marven Gilhespie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MACHINE_BOARD_RASPBERRY_PI_4_BOOT_H
#define MACHINE_BOARD_RASPBERRY_PI_4_BOOT_H

#include <kernel/board/arm.h>
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

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

#include <machine/cheviot_hal.h>
#include <string.h>
#include "bootstrap.h"
#include "bootinfo.h"
#include "debug.h"
#include "globals.h"
#include "aux_uart.h"
#include "debug.h"
#include "gpio.h"
#include "led.h"
#include "peripheral_base.h"

// Macros for address alignment. TODO: Replace with libc roundup and rounddown
#define ALIGN_UP(val, alignment)                                               \
  ((((val) + (alignment)-1) / (alignment)) * (alignment))

#define ALIGN_DOWN(val, alignment) ((val) - ((val) % (alignment)))



// Variables
uint32_t *root_pagetables;
uint32_t *root_pagedir;
uint32_t *kernel_pagetables;

// Prototypes
extern void *memset(void *dst, int value, size_t sz);
void InitPageDirectory(void);
void InitKernelPagetables(void);
void InitRootPagetables(void);


/* @brief   Pulls the kernel up by its boot laces.
 *
 * Kernel is loaded at 1MB (0x00100000) but is compiled to run at 0x80100000.
 * Sets up pagetables to map physical memory starting at 0 to 512MB into kernel
 * starting at 2GB (0x80000000).
 *
 * IO memory for screen, timer, interrupt and gpios are mapped at 0xA0000000.
 * Bootloader and loaded modules are identity mapped from 4k upto 478k.
 */
void BootstrapKernel(vm_addr kernel_ceiling)
{
  vm_addr pagetable_base;
  vm_addr pagetable_ceiling;
  vm_addr heap_ptr;

  heap_ptr = ALIGN_UP(kernel_ceiling, (16384));
  kernel_ceiling = heap_ptr;

  root_pagedir = (uint32_t *)heap_ptr;
  heap_ptr += 16384;

  root_pagetables = (uint32_t *)heap_ptr;
  heap_ptr += 4096;

  kernel_pagetables = (uint32_t *)heap_ptr;
  heap_ptr += 524288;

  pagetable_base = kernel_ceiling;
  pagetable_ceiling = heap_ptr;

  boot_log_info("BootstrapKernel");

  bootinfo.root_pagedir = (void *)((vm_addr)root_pagedir + KERNEL_BASE_VA);
  bootinfo.kernel_pagetables =
      (void *)((vm_addr)kernel_pagetables + KERNEL_BASE_VA);
  bootinfo.root_pagetables =
      (void *)((vm_addr)root_pagetables + KERNEL_BASE_VA);
  bootinfo.pagetable_base = pagetable_base + KERNEL_BASE_VA;
  bootinfo.pagetable_ceiling = pagetable_ceiling + KERNEL_BASE_VA;

  boot_log_info("root_pagedir = %08x", (vm_addr)root_pagedir);
  boot_log_info("bi.root_pagedir = %08x", (vm_addr)bootinfo.root_pagedir);
  boot_log_info("bi.kernel_pagetables = %08x", (vm_addr)bootinfo.kernel_pagetables);
  boot_log_info("bi.root_pagetables = %08x", (vm_addr)bootinfo.root_pagetables);
  boot_log_info("bi.pagetable_base = %08x", (vm_addr)bootinfo.pagetable_base);
  boot_log_info("bi.pagetable_ceiling = %08x", (vm_addr)bootinfo.pagetable_ceiling);

  InitPageDirectory();
  InitKernelPagetables();
  InitRootPagetables();

  uint32_t sctlr = hal_get_sctlr();
  uint32_t ttbcr = hal_get_ttbcr();
  uint32_t ttbr0 = hal_get_ttbr0();
  uint32_t cpsr  = hal_get_cpsr();

  boot_log_info("cpsr = %08x", cpsr);
  boot_log_info("sctlr = %08x", sctlr);
  boot_log_info("ttbcr = %08x", ttbcr);
  boot_log_info("ttbr0 = %08x", ttbr0);
  boot_log_info("---");


  boot_log_info("enable paging...");

  hal_set_ttbcr(0);
  hal_set_dacr(0x01);
  hal_set_ttbr0((uint32_t)root_pagedir);

  sctlr = hal_get_sctlr();
  sctlr |= SCTLR_M;    
  hal_set_sctlr(sctlr);

  boot_log_info(".. paging enabled");
}


/*
 * Initialises the page directory of the initial bootloader process.
 * Bootloader and IO pagetables have entries marked as not present
 * and a partially populated later.
 */
void InitPageDirectory(void) {
  uint32_t t;
  uint32_t *phys_pt;
  uint32_t pa;
  
  for (t = 0; t < 4096; t++) {
    root_pagedir[t] = L1_TYPE_INV;
  }

#if 1
  // For debugging we identity map the peripherals including AUX uart registers
  // so that we can continue to log to the serial port.
  // Within the kernel we later remap the aux_uart to an address somewhere just
  // above the kernel and these page tables.  We will eventually unmap these sections.
  for (t = 3840, pa=0xF0000000; t < N_PAGEDIR_PDE; t++, pa+= (1024*1024)) {
    root_pagedir[t] = pa | L1_TYPE_S | L1_S_AP_RWK;
  }
#endif

  for (t = 0; t < KERNEL_PAGETABLES_CNT; t++) {
    phys_pt = kernel_pagetables + t * N_PAGETABLE_PTE;
    root_pagedir[KERNEL_PAGETABLES_PDE_BASE + t] = (uint32_t)phys_pt | L1_TYPE_C;
  }

  for (int t = 0; t < ROOT_PAGETABLES_CNT; t++) {
    root_pagedir[t] = ((uint32_t)root_pagetables + t * PAGE_SIZE) | L1_TYPE_C;
  }
}


/*
 * Initialise the page table entries to map phyiscal memory from 0 to 512MB
 * into the kernel starting at 0x80000000.
 *
 * 
 */
void InitKernelPagetables(void) {
  uint32_t pa_bits;
  vm_addr pa;

  pa_bits = L2_TYPE_S | L2_AP_RWK;
//  pa_bits |= L2_B | L2_C;

  for (pa = 0; pa < bootinfo.mem_size; pa += PAGE_SIZE) {
    kernel_pagetables[pa / PAGE_SIZE] = pa | pa_bits;
  }
}


/* @brief   Initialize the page tables of the initial bootloader process
 *
 * Populate the page table for the root process (this bootloader).
 * Identity maps memory from 4k to 478k. In the kernel user processes
 * use 4k pagetables with 1k being the actual hardware defined pagetable
 * and the remaining 3k holding virtual-PTEs, for each page table entry.
 */
void InitRootPagetables(void)
{
  uint32_t pa_bits;
  vm_addr pa;
  uint32_t *pt;
  int pde_idx;
  int pte_idx;

  // Clear root pagetables (ptes and vptes)
  for (pde_idx = 0; pde_idx < ROOT_PAGETABLES_CNT; pde_idx++) {
    pt = (uint32_t *)(root_pagedir[pde_idx] & L1_C_ADDR_MASK);

    for (pte_idx = 0; pte_idx < N_PAGETABLE_PTE; pte_idx++) {
      pt[pte_idx] = L2_TYPE_INV;
    }

    memset((uint8_t *)pt + VPTE_TABLE_OFFS, 0, PAGE_SIZE - VPTE_TABLE_OFFS);
  }

  for (pa = 4096; pa < ROOT_CEILING_ADDR; pa += PAGE_SIZE) {
    pde_idx = (pa & L1_ADDR_BITS) >> L1_IDX_SHIFT;
    pt = (uint32_t *)(root_pagedir[pde_idx] & L1_C_ADDR_MASK);

    pte_idx = (pa & L2_ADDR_BITS) >> L2_IDX_SHIFT;

    pa_bits = L2_TYPE_S | L2_AP_RWKU;
      
//    pa_bits |= L2_B | L2_C;

    
    pt[pte_idx] = pa | pa_bits;
  }  
}


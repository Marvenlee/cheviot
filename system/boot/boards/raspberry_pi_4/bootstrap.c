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
#include "peripheral_base.h"

// Macros for address alignment. TODO: Replace with libc roundup and rounddown
#define ALIGN_UP(val, alignment)                                               \
  ((((val) + (alignment)-1) / (alignment)) * (alignment))

#define ALIGN_DOWN(val, alignment) ((val) - ((val) % (alignment)))


vm_addr io_map_base;
vm_addr io_map_current;

void *aux_regs_va;

// Variables
uint32_t *root_pagedir;
uint32_t *bootloader_pagetables;
uint32_t *kernel_pagetables;
uint32_t *io_pagetable;

// Prototypes
//extern void *memset(void *dst, int value, size_t sz);
void init_page_directory(void);
void init_kernel_pagetables(void);
void init_bootloader_pagetables(void);
void init_io_pagetable(void);
void *io_map(vm_addr pa, size_t sz);


/* @brief   Pulls the kernel up by its boot laces.
 *
 * Kernel is loaded at 1MB (0x00100000) but is compiled to run at 0x80100000.
 * Sets up pagetables to map physical memory starting at 0 to 512MB into kernel
 * starting at 2GB (0x80000000).
 *
 * IO memory for screen, timer, interrupt and gpios are mapped at 0xA0000000.
 * Bootloader and loaded modules are identity mapped from 4k upto 478k.
 */
void bootstrap_kernel(vm_addr kernel_ceiling)
{
  vm_addr pagetable_base;
  vm_addr pagetable_ceiling;
  vm_addr heap_ptr;

  heap_ptr = ALIGN_UP(kernel_ceiling, (16384));
  kernel_ceiling = heap_ptr;

	// Reserve pages for kernel pagetables and pagetables for bootloader
  pagetable_base = kernel_ceiling;

  root_pagedir = (uint32_t *)heap_ptr;
  heap_ptr += 16384;

  bootloader_pagetables = (uint32_t *)heap_ptr;
  heap_ptr += 4096;

  kernel_pagetables = (uint32_t *)heap_ptr;
  heap_ptr += 2097152;				// TODO: Scale this based on amount of memory

	io_pagetable = (uint32_t *)heap_ptr;
	heap_ptr += 4096;

  pagetable_ceiling = heap_ptr;

  boot_log_info("BootstrapKernel");

  bootinfo.root_pagedir = (void *)((vm_addr)root_pagedir + VM_KERNEL_BASE);
  bootinfo.kernel_pagetables =
      (void *)((vm_addr)kernel_pagetables + VM_KERNEL_BASE);
  bootinfo.bootloader_pagetables =
      (void *)((vm_addr)bootloader_pagetables + VM_KERNEL_BASE);
  bootinfo.pagetable_base = pagetable_base + VM_KERNEL_BASE;
  bootinfo.pagetable_ceiling = pagetable_ceiling + VM_KERNEL_BASE;

  boot_log_info("root_pagedir = %08x", (vm_addr)root_pagedir);
  boot_log_info("bi.root_pagedir = %08x", (vm_addr)bootinfo.root_pagedir);
  boot_log_info("bi.kernel_pagetables = %08x", (vm_addr)bootinfo.kernel_pagetables);
  boot_log_info("bi.bootloader_pagetables = %08x", (vm_addr)bootinfo.bootloader_pagetables);
  boot_log_info("bi.pagetable_base = %08x", (vm_addr)bootinfo.pagetable_base);
  boot_log_info("bi.pagetable_ceiling = %08x", (vm_addr)bootinfo.pagetable_ceiling);

	boot_log_info("io_pagetable: %08x", (vm_addr)io_pagetable);

  init_page_directory();
  init_kernel_pagetables();
  init_bootloader_pagetables();
	init_io_pagetable();
	
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
  hal_set_dacr(0x01);  // FIXME: Set all domains with 0x55555555
  hal_set_ttbr0((uint32_t)root_pagedir);

  sctlr = hal_get_sctlr();
  sctlr |= SCTLR_M | SCTLR_C | SCTLR_I;    
  hal_set_sctlr(sctlr);
  hal_dsb();
  hal_isb();

	aux_regs = aux_regs_va;

  boot_log_info(".. paging enabled");
}


/*
 * Initialises the page directory of the initial bootloader process.
 * Bootloader and IO pagetables have entries marked as not present
 * and a partially populated later.
 */
void init_page_directory(void)
{
  uint32_t t;
  uint32_t *phys_pt;
  uint32_t pa;
  
  for (t = 0; t < N_PAGEDIR_PDE; t++) {
    root_pagedir[t] = L1_TYPE_INV;
  }

  root_pagedir[0] = ((uint32_t)bootloader_pagetables) | L1_TYPE_C;
  
  for (t = 0; t < KERNEL_PAGETABLES_CNT; t++) {
    phys_pt = kernel_pagetables + t * N_PAGETABLE_PTE;
    root_pagedir[KERNEL_PAGETABLES_PDE_BASE + t] = (uint32_t)phys_pt | L1_TYPE_C;
  }
  
	root_pagedir[IO_PAGETABLES_PDE_BASE] = ((uint32_t)io_pagetable) | L1_TYPE_C;  
}


/*
 * Initialise the page table entries to map phyiscal memory from 0 to 512MB
 * into the kernel starting at 0x80000000.
 */
void init_kernel_pagetables(void)
{
  uint32_t pa_bits;
  vm_addr pa;

  pa_bits = L2_TYPE_S | L2_AP_RWK;
	//  pa_bits |= L2_B | L2_C;			// FIXME

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
void init_bootloader_pagetables(void)
{
  uint32_t pa_bits;
  vm_addr pa;
  uint32_t *pt;
  int pde_idx;
  int pte_idx;

  // Clear root pagetables (ptes and vptes)
  for (pde_idx = 0; pde_idx < BOOTLOADER_PAGETABLES_CNT; pde_idx++) {
    pt = (uint32_t *)(root_pagedir[pde_idx] & L1_C_ADDR_MASK);

    for (pte_idx = 0; pte_idx < N_PAGETABLE_PTE; pte_idx++) {
      pt[pte_idx] = L2_TYPE_INV;
    }

    memset((uint8_t *)pt + VPTE_TABLE_OFFS, 0, PAGE_SIZE - VPTE_TABLE_OFFS);
  }

  for (pa = 4096; pa < VM_BOOTLOADER_CEILING; pa += PAGE_SIZE) {
    pde_idx = (pa & L1_ADDR_BITS) >> L1_IDX_SHIFT;
    pt = (uint32_t *)(root_pagedir[pde_idx] & L1_C_ADDR_MASK);

    pte_idx = (pa & L2_ADDR_BITS) >> L2_IDX_SHIFT;

    pa_bits = L2_TYPE_S | L2_AP_RWKU;
    // pa_bits |= L2_B | L2_C;   // FIXME:
    
    pt[pte_idx] = pa | pa_bits;
  }  
}


/* @brief   Initialize pagetable that maps IO registers used by kernel
 *
 */
void init_io_pagetable(void)
{
  uint32_t *phys_pt;

  for (int t = 0; t < N_PAGETABLE_PTE; t++) {
    io_pagetable[t] = L2_TYPE_INV;
  }

  io_map_base = VM_IO_BASE;
  io_map_current = io_map_base;
  
  bootinfo.timer_base = (vm_addr)io_map(TIMER_BASE, PAGE_SIZE);
  bootinfo.gpio_base  = (vm_addr)io_map(GPIO_BASE, PAGE_SIZE);
  bootinfo.aux_base   = (vm_addr)io_map(AUX_BASE, PAGE_SIZE);	
  bootinfo.gicd_base  = (vm_addr)io_map(GICD_BASE, PAGE_SIZE);
  bootinfo.gicc_base[0] = (vm_addr)io_map(GICC_BASE, PAGE_SIZE);
	
	aux_regs_va = (void *)bootinfo.aux_base;

	vm_addr mbox_page = (MBOX_BASE / PAGE_SIZE) * PAGE_SIZE;
	vm_addr mbox_page_offset = MBOX_BASE % PAGE_SIZE;	

  void *mbox_base    = io_map(mbox_page, PAGE_SIZE);   
	mbox_base = (uint8_t *)mbox_base + mbox_page_offset;

	bootinfo.mailbox_base = (vm_addr)mbox_base;
	hal_set_mbox_base(mbox_base);  

	boot_log_info("bi.timer_base:   %08x", (uint32_t)bootinfo.timer_base);
	boot_log_info("bi.gpio_base:    %08x", (uint32_t)bootinfo.gpio_base);
	boot_log_info("bi.aux_base:     %08x", (uint32_t)bootinfo.aux_base);
	boot_log_info("bi.gicd_base:    %08x", (uint32_t)bootinfo.gicd_base);
	boot_log_info("bi.gicc_base[0]: %08x", (uint32_t)bootinfo.gicc_base[0]);
	boot_log_info("bi.mailbox_base: %08x", (uint32_t)bootinfo.mailbox_base);

}


/* @brief   Map a region of memory into the IO region above 0xA0000000.
 */
void *io_map(vm_addr pa, size_t sz)
{
  int pte_idx;
  uint32_t pa_bits;
  vm_addr pa_base, pa_ceiling;
  vm_addr va_base, va_ceiling;
  vm_addr va;
  
  boot_log_info("map io, pa: %08x, sz:%08x", (uint32_t)pa, (uint32_t)sz);

  pa_base = ALIGN_DOWN(pa, PAGE_SIZE);
  pa_ceiling = ALIGN_UP(pa + sz, PAGE_SIZE);

  va_base = ALIGN_UP(io_map_current, PAGE_SIZE);
  va_ceiling = va_base + (pa_ceiling - pa_base);

  pa_bits = L2_TYPE_S | L2_AP_RWK;    // read/write kernel-only
  // pa_bits |= ;   TODO:  Do we need to set any bits cache/bufferable for i/o pages?

  for (va = va_base, pa = pa_base; pa < pa_ceiling; va+= PAGE_SIZE, pa += PAGE_SIZE) {
	  pte_idx = (va - io_map_base) / PAGE_SIZE;
    io_pagetable[pte_idx] = pa | pa_bits;  
  }

  io_map_current = va_ceiling;

  return (void *)va_base;
}



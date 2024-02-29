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

#include <kernel/arch.h>
#include <kernel/board/peripheral_base.h>
#include <kernel/board/boot.h>
#include <kernel/board/globals.h>
#include <kernel/board/init.h>
#include <kernel/board/aux_uart.h>
#include <kernel/board/interrupt.h>
#include <kernel/board/gpio.h>
#include <kernel/board/timer.h>
#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/lists.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>
//#include <machine/cheviot_hal.h>

// FIXME: Move elsewhere?   Needed for interrupt, timer, gpio and uart.
// Can remove UART once sure of booting
#define IO_PAGETABLES_CNT 16
#define IO_PAGETABLES_PDE_BASE 2560
#define io_map_BASE_VA 0xA0000000

vm_addr io_map_base;
vm_addr io_map_current;


/* @brief   Initialize the virual memory management system.
 *
 * Note that all of physical memory is mapped into the kernel along with areas
 * for peripheral registers for timers and GPIOs. for LEDs.
 */
void init_vm(void)
{
  vm_addr pa;

  init_memory_map();

  boot_base = BOOT_BASE_ADDR;
  boot_ceiling = BOOT_CEILING_ADDR;

  // Do core pagetables base also include root page tables?
  // core_pagetable_base = VirtToPhys((vm_addr)bootinfo->io_pagetables);

  core_pagetable_base = bootinfo->pagetable_base;
  core_pagetable_ceiling = bootinfo->pagetable_ceiling;

  // 0 to boot base, is this first 4k ?
  init_pageframe_flags((vm_addr)0, (vm_addr)boot_base, PGF_KERNEL | PGF_INUSE);

  // boot_base to boot ceiling?  bootsector 4k to 64k ?
  // FIXME: release lower 4k-64k  BootSector,  IFS image, etc is at top of memory?
  init_pageframe_flags(boot_base, boot_ceiling, PGF_KERNEL | PGF_INUSE);
      
  // Reserve kernel pagetables from start of text (0x80100000) to ceiling of kernel's bss.      
  init_pageframe_flags(VirtToPhys((vm_addr)&_stext), VirtToPhys((vm_addr)&_ebss), PGF_KERNEL | PGF_INUSE);

  // Core page tables allocated above kernel > 1MB, below heap which is allocated after 
  init_pageframe_flags(VirtToPhys(core_pagetable_base), VirtToPhys(core_pagetable_ceiling), PGF_KERNEL | PGF_INUSE);

  // Reserve pages for kernel heap
  init_pageframe_flags(VirtToPhys(_heap_base), VirtToPhys(_heap_current), PGF_KERNEL | PGF_INUSE);  

  // Mark pages from top of kernel's heap to ifs_exe_base as free
  init_pageframe_flags(VirtToPhys(_heap_current), (vm_addr)bootinfo->ifs_exe_base, 0);

  // Mark IFS image pages as in-use. This will be mapped into root process BootstrapRootProcess.
  init_pageframe_flags(bootinfo->ifs_image, bootinfo->ifs_image_size, PGF_INUSE);

  // Error checking, all pages should have 0 or 1 references.
  for (pa = 0; pa < mem_size; pa += PAGE_SIZE) {
    KASSERT(pageframe_table[pa / PAGE_SIZE].reference_cnt <= 1);
  }
  
  coalesce_free_pageframes();
}


/* @brief   Initialise the pageframes in the memory map
 */
void init_memory_map(void)
{
  for (int t = 0; t < max_pageframe; t++) {
    pageframe_table[t].size = PAGE_SIZE;
    pageframe_table[t].physical_addr = t * PAGE_SIZE;
    pageframe_table[t].reference_cnt = 0;
    pageframe_table[t].flags = 0;
    pmap_pageframe_init(&pageframe_table[t].pmap_pageframe);
  }
}

/*!
*/
void init_pageframe_flags(vm_addr base, vm_addr ceiling, bits32_t flags)
{
  vm_addr pa;

  base = ALIGN_DOWN(base, PAGE_SIZE);
  ceiling = ALIGN_UP(ceiling, PAGE_SIZE);

  for (pa = base; pa < ceiling; pa += PAGE_SIZE) {
    pageframe_table[pa / PAGE_SIZE].flags = flags;

    // FIXME: ONLY FOR USER-MODE PAGES.  PAGE TABLES HAVE PTE COUNT
    // pageframe_table[pa / PAGE_SIZE].reference_cnt++;
  }
}

/* @brief   Coalesce free pageframes into larger blocks
 */
void coalesce_free_pageframes(void)
{
  vm_addr pa;
  vm_addr pa2;
  int slab_free_page_cnt;
  int cnt = 0;
  struct Pageframe *pf;

  LIST_INIT(&free_4k_pf_list);
  LIST_INIT(&free_16k_pf_list);
  LIST_INIT(&free_64k_pf_list);

  for (pa = 0; pa + 0x10000 < mem_size; pa += 0x10000) {
    slab_free_page_cnt = 0;

    for (pa2 = pa; pa2 < pa + 0x10000; pa2 += PAGE_SIZE) {
      KASSERT(pageframe_table[pa2 / PAGE_SIZE].flags == 0 ||
              pageframe_table[pa2 / PAGE_SIZE].flags & PGF_USER ||
              pageframe_table[pa2 / PAGE_SIZE].flags & PGF_KERNEL);

      if (pageframe_table[pa2 / PAGE_SIZE].flags == 0) {
        slab_free_page_cnt++;
      }
    }

    if (slab_free_page_cnt * PAGE_SIZE == 0x10000) {
      pageframe_table[pa / PAGE_SIZE].size = 0x10000;
      LIST_ADD_TAIL(&free_64k_pf_list, &pageframe_table[pa / PAGE_SIZE], link);
    } else {
      for (pa2 = pa; pa2 < pa + 0x10000; pa2 += PAGE_SIZE) {
        if (pageframe_table[pa2 / PAGE_SIZE].flags == 0) {
          pageframe_table[pa2 / PAGE_SIZE].size = PAGE_SIZE;
          LIST_ADD_TAIL(&free_4k_pf_list, &pageframe_table[pa2 / PAGE_SIZE],
                        link);
        }
      }
    }
  }

  /*
    // Last remaining pages that don't make up a whole 64k
    if (pa < mem_size) {
        for (pa2 = pa; pa2 < mem_size; pa2 += PAGE_SIZE) {
            if (pageframe_table[pa2/PAGE_SIZE].flags == 0) {
                pageframe_table[pa2/PAGE_SIZE].size = PAGE_SIZE;
                LIST_ADD_TAIL (&free_4k_pf_list,
   &pageframe_table[pa2/PAGE_SIZE], link);
            }
        }
    }
*/

  pf = LIST_HEAD(&free_4k_pf_list);
  while (pf != NULL) {
    cnt++;
    pf = LIST_NEXT(pf, link);
  }

  pf = LIST_HEAD(&free_16k_pf_list);
  while (pf != NULL) {
    cnt++;
    pf = LIST_NEXT(pf, link);
  }

  pf = LIST_HEAD(&free_64k_pf_list);
  while (pf != NULL) {
    cnt++;
    pf = LIST_NEXT(pf, link);
  }
}


/* @brief   Initialize kernel pagetables for mapping IO areas
 */
void init_io_pagetables(void)
{
  io_map_base = io_map_BASE_VA;
  io_map_current = io_map_base;
  uint32_t *phys_pt;

  root_pagedir = bootinfo->root_pagedir;

  for (int t = 0; t < IO_PAGETABLES_CNT; t++) {
    phys_pt =
        (uint32_t *)pmap_va_to_pa((vm_addr)(io_pagetable + t * N_PAGETABLE_PTE));
    root_pagedir[IO_PAGETABLES_PDE_BASE + t] = (uint32_t)phys_pt | L1_TYPE_C;
  }

  for (int t = 0; t < IO_PAGETABLES_CNT * N_PAGETABLE_PTE; t++) {
    io_pagetable[t] = L2_TYPE_INV;
  }

  timer_regs     = io_map(TIMER_BASE, sizeof(struct bcm2835_timer_registers), false);
  interrupt_regs = io_map(ARMCTRL_BASE, sizeof(struct bcm2835_interrupt_registers), false);
  gpio_regs      = io_map(GPIO_BASE, sizeof(struct bcm2835_gpio_registers), false);
  aux_regs       = io_map(AUX_BASE, sizeof(struct bcm2835_aux_registers), false);

  // Interrupt registers are 0x200 above page aligned base */
  interrupt_regs = (struct bcm2835_interrupt_registers *)((uint8_t*)interrupt_regs + ARMCTRL_INTC_BASE);

  hal_set_page_directory((void *)(pmap_va_to_pa((vm_addr)root_pagedir)));
  hal_invalidate_tlb();
  hal_flush_all_caches();
}


/* @brief   Map a region of memory into the IO region above 0xA0000000.
 *
 * TODO:  Add flags, field, screen can be write, combine others not so.
 */
void *io_map(vm_addr pa, size_t sz, bool bufferable)
{
  int pte_idx;
  uint32_t pa_bits;
  vm_addr pa_base, pa_ceiling;
  vm_addr va_base, va_ceiling;
  vm_addr regs;
  vm_addr paddr;

  pa_base = ALIGN_DOWN(pa, PAGE_SIZE);
  pa_ceiling = ALIGN_UP(pa + sz, PAGE_SIZE);

  va_base = ALIGN_UP(io_map_current, PAGE_SIZE);
  va_ceiling = va_base + (pa_ceiling - pa_base);
  regs = (va_base + (pa - pa_base));

  pa_bits = L2_TYPE_S |
            L2_AP(AP_W); // TODO:  Any way to make write-combine/write thru ?

  if (bufferable) {
    pa_bits |= L2_B;
  }

  paddr = pa_base;
  pte_idx = (va_base - io_map_base) / PAGE_SIZE;

  for (paddr = pa_base; paddr < pa_ceiling; paddr += PAGE_SIZE) {
    io_pagetable[pte_idx] = paddr | pa_bits;
    pte_idx++;
  }

  io_map_current = va_ceiling;

  return (void *)regs;
}

/**
 *
 */
void init_buffer_cache_pagetables(void)
{
  uint32_t *phys_pt;
  struct PmapVPTE *vpte;
  struct PmapVPTE *vpte_base;
  uint32_t *pt;

  root_pagedir = bootinfo->root_pagedir;

  for (int t = 0; t < CACHE_PAGETABLES_CNT; t++) {
    phys_pt =
        (uint32_t *)pmap_va_to_pa((vm_addr)(cache_pagetable + t * VPAGETABLE_SZ));
    root_pagedir[CACHE_PAGETABLES_PDE_BASE + t] = (uint32_t)phys_pt | L1_TYPE_C;

    pt = (uint32_t *)((vm_addr)(cache_pagetable + t * VPAGETABLE_SZ));

    for (int pte_idx = 0; pte_idx < N_PAGETABLE_PTE; pte_idx++) {
      vpte_base = (struct PmapVPTE *)((uint8_t *)pt + VPTE_TABLE_OFFS);
      vpte = vpte_base + pte_idx;

      pt[pte_idx] = L2_TYPE_INV;
      vpte->flags = 0;
      vpte->link.prev = NULL;
      vpte->link.next = NULL;
    }
  }

  // FIXME: Why are we changing page directory and flushing caches here?
  hal_set_page_directory((void *)(pmap_va_to_pa((vm_addr)root_pagedir)));
  hal_invalidate_tlb();
  hal_flush_all_caches();
}


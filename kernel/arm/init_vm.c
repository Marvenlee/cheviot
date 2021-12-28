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
#include <kernel/arm/boot.h>
#include <kernel/arm/globals.h>
#include <kernel/arm/init.h>
#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/lists.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>


// FIXME: Move elsewhere?

#define IO_PAGETABLES_CNT 16
#define IO_PAGETABLES_PDE_BASE 2560
#define IOMAP_BASE_VA 0xA0000000
vm_addr iomap_base;
vm_addr iomap_current;


/*!
 *   Initialize the virual memory management system.
 *
 *   Note that all of physical memory is mapped into the kernel along with areas
 *   for peripheral registers for timers and GPIOs. for LEDs.
 */
void InitVM(void) {
  vm_addr pa;

  InitMemoryMap();

  root_base = BOOT_BASE_ADDR;
  root_ceiling = BOOT_CEILING_ADDR;

  // Do core pagetables base also include root page tables?
  //    core_pagetable_base = VirtToPhys((vm_addr)bootinfo->io_pagetables);

  core_pagetable_base = bootinfo->pagetable_base;
  core_pagetable_ceiling = bootinfo->pagetable_ceiling;
  InitPageframeFlags((vm_addr)0, (vm_addr)root_base, PGF_KERNEL | PGF_INUSE);

  // FIXME:  Need to copy bootinfo into kernel, only use it's copy.  Then can
  // release lower 4k-64k
    
  InitPageframeFlags(root_base, root_ceiling, PGF_KERNEL | PGF_INUSE);
  InitPageframeFlags(VirtToPhys((vm_addr)&_stext), VirtToPhys((vm_addr)&_ebss), PGF_KERNEL | PGF_INUSE);
  InitPageframeFlags(VirtToPhys(core_pagetable_base), VirtToPhys(core_pagetable_ceiling), PGF_KERNEL | PGF_INUSE);
  InitPageframeFlags(VirtToPhys(_heap_base), VirtToPhys(_heap_current), PGF_KERNEL | PGF_INUSE);  
  InitPageframeFlags(VirtToPhys(_heap_current), (vm_addr)mem_size, 0);

  for (pa = 0; pa < mem_size; pa += PAGE_SIZE) {
    KASSERT(pageframe_table[pa / PAGE_SIZE].reference_cnt <= 1);
  }

  // Unmap root pagetables();
  
  // Map loader and map root process
  
  // Reload page directory

  CoalesceFreePageframes();
}

/*!
*/
void InitMemoryMap(void) {
  for (int t = 0; t < max_pageframe; t++) {
    pageframe_table[t].size = PAGE_SIZE;
    pageframe_table[t].physical_addr = t * PAGE_SIZE;
    pageframe_table[t].reference_cnt = 0;
    pageframe_table[t].flags = 0;
    PmapPageframeInit(&pageframe_table[t].pmap_pageframe);
  }
}

/*!
*/
void InitPageframeFlags(vm_addr base, vm_addr ceiling, bits32_t flags) {
  vm_addr pa;

  base = ALIGN_DOWN(base, PAGE_SIZE);
  ceiling = ALIGN_UP(ceiling, PAGE_SIZE);

  for (pa = base; pa < ceiling; pa += PAGE_SIZE) {
    pageframe_table[pa / PAGE_SIZE].flags = flags;

    // FIXME: ONLY FOR USER-MODE PAGES.  PAGE TABLES HAVE PTE COUNT
    pageframe_table[pa / PAGE_SIZE].reference_cnt++;
  }
}

/*!
*/
void CoalesceFreePageframes(void) {
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

/*!
*/
void InitIOPagetables(void) {
  iomap_base = IOMAP_BASE_VA;
  iomap_current = iomap_base;
  uint32_t *phys_pt;

  root_pagedir = bootinfo->root_pagedir;

  for (int t = 0; t < IO_PAGETABLES_CNT; t++) {
    phys_pt =
        (uint32_t *)PmapVaToPa((vm_addr)(io_pagetable + t * N_PAGETABLE_PTE));
    root_pagedir[IO_PAGETABLES_PDE_BASE + t] = (uint32_t)phys_pt | L1_TYPE_C;
  }

  for (int t = 0; t < IO_PAGETABLES_CNT * N_PAGETABLE_PTE; t++) {
    io_pagetable[t] = L2_TYPE_INV;
  }

  screen_buf = IOMap((vm_addr)bootinfo->screen_buf,
                     bootinfo->screen_pitch * bootinfo->screen_height, TRUE);
  screen_width = bootinfo->screen_width;
  screen_height = bootinfo->screen_height;
  screen_pitch = bootinfo->screen_pitch;

  timer_regs =
      IOMap(ST_BASE, sizeof(struct bcm2835_system_timer_registers), FALSE);
  interrupt_regs =
      IOMap(ARMCTRL_IC_BASE, sizeof(struct bcm2835_interrupt_registers), FALSE);
  gpio_regs = IOMap(GPIO_BASE, sizeof(struct bcm2835_gpio_registers), FALSE);

  uart_regs = IOMap(UART_BASE, sizeof(struct bcm2835_uart_registers), FALSE);

  SetPageDirectory((void *)(PmapVaToPa((vm_addr)root_pagedir)));
  InvalidateTLB();
  FlushAllCaches();
}

/*!
    Map a region of memory into the IO region above 0xA0000000.
    TODO:  Add flags, field, screen can be write, combine others not so.
*/
void *IOMap(vm_addr pa, size_t sz, bool bufferable) {
  int pte_idx;
  uint32_t pa_bits;
  vm_addr pa_base, pa_ceiling;
  vm_addr va_base, va_ceiling;
  vm_addr regs;
  vm_addr paddr;

  pa_base = ALIGN_DOWN(pa, PAGE_SIZE);
  pa_ceiling = ALIGN_UP(pa + sz, PAGE_SIZE);

  va_base = ALIGN_UP(iomap_current, PAGE_SIZE);
  va_ceiling = va_base + (pa_ceiling - pa_base);
  regs = (va_base + (pa - pa_base));

  pa_bits = L2_TYPE_S |
            L2_AP(AP_W); // TODO:  Any way to make write-combine/write thru ?

  if (bufferable) {
    pa_bits |= L2_B;
  }

  paddr = pa_base;
  pte_idx = (va_base - iomap_base) / PAGE_SIZE;

  for (paddr = pa_base; paddr < pa_ceiling; paddr += PAGE_SIZE) {
    io_pagetable[pte_idx] = paddr | pa_bits;
    pte_idx++;
  }

  iomap_current = va_ceiling;

  return (void *)regs;
}

/**
 *
 */
void InitBufferCachePagetables(void) {
  uint32_t *phys_pt;
  struct PmapVPTE *vpte;
  struct PmapVPTE *vpte_base;
  uint32_t *pt;

  root_pagedir = bootinfo->root_pagedir;

  for (int t = 0; t < CACHE_PAGETABLES_CNT; t++) {
    phys_pt =
        (uint32_t *)PmapVaToPa((vm_addr)(cache_pagetable + t * VPAGETABLE_SZ));
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

  SetPageDirectory((void *)(PmapVaToPa((vm_addr)root_pagedir)));
  InvalidateTLB();
  FlushAllCaches();
}

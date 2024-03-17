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

#define KDEBUG

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
#include <machine/cheviot_hal.h>


/*
 *
 */
void init_io_addresses(void)
{
  timer_regs = bootinfo->timer_base;
  gpio_regs  = bootinfo->gpio_base;
  aux_regs   = bootinfo->aux_base;  

	hal_set_mbox_base(bootinfo->mailbox_base);  
	
  gic_dist_regs      = bootinfo->gicd_base;
  gic_cpu_iface_regs = bootinfo->gicc_base[0];
}


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
  core_pagetable_base = bootinfo->pagetable_base;
  core_pagetable_ceiling = bootinfo->pagetable_ceiling;

  // 0 to boot base, is this first 4k ?
  init_pageframe_flags((vm_addr)0, (vm_addr)boot_base, PGF_KERNEL | PGF_INUSE);

  // boot_base to boot ceiling.  bootsector 0k to 64k ?
  init_pageframe_flags(boot_base, boot_ceiling, PGF_KERNEL | PGF_INUSE);
  
  // Reserve memory for videocore      
  init_pageframe_flags(bootinfo->videocore_base, bootinfo->videocore_ceiling, PGF_INUSE);
        
  // Reserve kernel pages from start of text 0x80010000 (2MB+64k) to ceiling of kernel's bss.      
  // This is from 64k phys to physical address of end of ebss.
  init_pageframe_flags(VirtToPhys((vm_addr)&_stext), VirtToPhys((vm_addr)&_ebss), PGF_KERNEL | PGF_INUSE);

  // Core page tables allocated above kernel > 1MB, below heap which is allocated after 
  init_pageframe_flags(VirtToPhys(core_pagetable_base), VirtToPhys(core_pagetable_ceiling), PGF_KERNEL | PGF_INUSE);
    
  // Reserve pages for kernel heap
  init_pageframe_flags(VirtToPhys(_heap_base), VirtToPhys(_heap_current), PGF_KERNEL | PGF_INUSE);  

  // Mark pages from top of kernel's heap to ifs_exe_base as free
  init_pageframe_flags(VirtToPhys(_heap_current), (vm_addr)bootinfo->ifs_exe_base, 0);

  // Mark IFS image pages as in-use. This will be mapped into root process BootstrapRootProcess.
//  init_pageframe_flags(bootinfo->ifs_image, bootinfo->ifs_image + bootinfo->ifs_image_size, PGF_INUSE);
  init_pageframe_flags(bootinfo->ifs_image, bootinfo->mem_size, PGF_INUSE);

  Info ("reserved from 0 to boot base");
  Info ("reserved boot base : %08x", boot_base);
  Info ("reserved boot ceil : %08x", boot_ceiling);
  Info ("reserved _stext : %08x", VirtToPhys((uint32_t)&_stext));
  Info ("reserved _ebss  : %08x", VirtToPhys((uint32_t)&_ebss));
  Info ("reserved core pt base : %08x", VirtToPhys(core_pagetable_base));
  Info ("reserved core pt ceil : %08x", VirtToPhys(core_pagetable_ceiling));
  Info ("reserved kernel heap base : %08x", VirtToPhys(_heap_base));
  Info ("reserved kernel heap ceil : %08x", VirtToPhys(_heap_current));
  Info ("reserved videocore base : %08x", bootinfo->videocore_base);
  Info ("reserved videocore ceil : %08x", bootinfo->videocore_base);
  Info ("reserved IFS base : %08x", bootinfo->ifs_image);
//  Info ("reserved IFS ceil : %08x", bootinfo->ifs_image + bootinfo->ifs_image_size);
  Info ("reserved IFS ceil : %08x", bootinfo->ifs_image + bootinfo->mem_size);

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


/*
 */
void init_pageframe_flags(vm_addr base, vm_addr ceiling, bits32_t flags)
{
  vm_addr pa;

  base = ALIGN_DOWN(base, PAGE_SIZE);
  ceiling = ALIGN_UP(ceiling, PAGE_SIZE);

  for (pa = base; pa < ceiling; pa += PAGE_SIZE) {
    pageframe_table[pa / PAGE_SIZE].flags = flags;

		// No reserved pages are reclaimable.  We cannot free any of the
		// the memory so reference counts and VPTEs are unused.
  }
}


/* @brief   Coalesce free 4k pageframes into larger 64k blocks
 */
void coalesce_free_pageframes(void)
{
  vm_addr pa;
  vm_addr pa2;

  LIST_INIT(&free_4k_pf_list);
  LIST_INIT(&free_16k_pf_list);
  LIST_INIT(&free_64k_pf_list);

  for (pa = 0; pa + 0x10000 <= mem_size; pa += 0x10000) {
    int slab_free_page_cnt = 0;

    for (pa2 = pa; pa2 < pa + 0x10000; pa2 += PAGE_SIZE) {
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
          LIST_ADD_TAIL(&free_4k_pf_list, &pageframe_table[pa2 / PAGE_SIZE], link);
        }
      }
    }
  }
}



/*
 * FIXME: Remove fixed area of kernel for buffer cache, dynamically
 * allocate pages with alloc_page(page_size);
 */
void init_buffer_cache_pagetables(void)
{
  uint32_t *phys_pt;
  struct PmapVPTE *vpte;
  struct PmapVPTE *vpte_base;
  uint32_t *pt;
  vm_addr va;
  
  root_pagedir = bootinfo->root_pagedir;

  va = CACHE_BASE_VA;
  
  for (int t = 0; t < CACHE_PAGETABLES_CNT; t++) {
    phys_pt = (uint32_t *)pmap_va_to_pa((vm_addr)(cache_pagetable + t * VPAGETABLE_SZ));
    root_pagedir[CACHE_PAGETABLES_PDE_BASE + t] = (uint32_t)phys_pt | L1_TYPE_C;

    pt = (uint32_t *)((vm_addr)(cache_pagetable + t * VPAGETABLE_SZ));

    for (int pte_idx = 0; pte_idx < N_PAGETABLE_PTE; pte_idx++) {
      vpte_base = (struct PmapVPTE *)((uint8_t *)pt + VPTE_TABLE_OFFS);
      vpte = vpte_base + pte_idx;

      pt[pte_idx] = L2_TYPE_INV;

    	hal_dsb();
	    hal_invalidate_tlb_va(va & ~0xfff);
      hal_invalidate_branch();
      hal_invalidate_icache();
      hal_dsb();
      hal_isb();

      vpte->flags = 0;
      vpte->link.prev = NULL;
      vpte->link.next = NULL;
      
      va += PAGE_SIZE;
    }
  }
}


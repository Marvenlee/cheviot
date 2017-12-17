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

#include <kernel/types.h>
#include <kernel/lists.h>
#include <kernel/vm.h>
#include <kernel/proc.h>
#include <kernel/dbg.h>
#include <kernel/utility.h>
#include <kernel/arch.h>
#include <kernel/arm/init.h>
#include <kernel/arm/boot.h>
#include <kernel/globals.h>


// Externs
extern uint8 _stext;
extern uint8 _ebss;
extern vm_addr _heap_base;
extern vm_addr _heap_current;

// Prototypes
void InitMemoryMap (void);
void InitRootPagetableVPTEs(void);
void InitPageframeFlags (vm_addr base, vm_addr ceiling, bits32_t flags);
void CoalesceFreePageframes (void);
void InitPageCache (void);


/*!
    Initialize the virual memory management system.
*/
void InitVM (void)
{    
    KLog ("InitVM()");

    InitMemoryMap();
    KLog (".. InitMemoryMap DONE");
    
    InitPageframeFlags ((vm_addr)0, (vm_addr)4096, PGF_KERNEL);
    InitPageframeFlags ((vm_addr)4096, (vm_addr)bootinfo->io_pagetables, PGF_USER);
    InitPageframeFlags ((vm_addr)bootinfo->io_pagetables, (vm_addr)0x00100000, PGF_USER);
    InitPageframeFlags (VirtToPhys((vm_addr)&_stext), VirtToPhys(_heap_current),  PGF_KERNEL);
    KLog (".. PopulateMemoryMap DONE");

    InitRootPagetableVPTEs();
    KLog (".. InitRootPagetableVPTEs DONE");

    CoalesceFreePageframes();
    KLog (".. PopulateFreePageframeLists DONE");

//    InitPageCache();    

    KLog ("InitVM() DONE");
}


/*!
*/
void InitMemoryMap (void)
{
    for (int t=0; t < max_pageframe; t++)
    {
        pageframe_table[t].size = PAGE_SIZE;
        pageframe_table[t].physical_addr = t * PAGE_SIZE;
        pageframe_table[t].reference_cnt = 0;
        pageframe_table[t].flags = 0;        
        PmapPageframeInit (&pageframe_table[t].pmap_pageframe);
    }
}


/*!
*/
void InitPageframeFlags (vm_addr base, vm_addr ceiling, bits32_t flags)
{
    vm_addr pa;

    base = ALIGN_DOWN (base, PAGE_SIZE);
    ceiling = ALIGN_UP (ceiling, PAGE_SIZE);
    
    for (pa = base; pa < ceiling; pa += PAGE_SIZE)
    {
        pageframe_table[pa/PAGE_SIZE].flags = flags;
    }
}


/*!
    Initialize the VPTEs of the root process.  Pagetables on ARM processors are
    normally 1k in size, holding 256 4-byte page table entries.  We use 4K pages
    to hold a 1k pagetable and a 3k virtual-pagetable above it.  This virtual-
    pagetable contains 4 bytes of extra flags used by the OS and linked list next/prev
    pointers (4 bytes each) to attach the VPTEs to a pageframe.  This makes it easier
    to track which pagetables a given Pageframe is mapped to and to do any remapping
    when needed. 
*/
void InitRootPagetableVPTEs(void)
{
    vm_addr pa;
    struct PmapVPTE *vpte_base;
    int pde_idx;
    int pte_idx;
    uint32 *phys_pt;
    uint32 *pt;
    uint32 *pd;
    
    pd = bootinfo->root_pagedir;
    pde_idx = (4096 & L1_ADDR_BITS) >> L1_IDX_SHIFT;
    phys_pt = (uint32 *) (pd[pde_idx] & L1_C_ADDR_MASK);                
    pt = (uint32 *)PhysToVirt ((vm_addr)phys_pt);

    // FIXME: Need User-base and User-Ceiling passed from bootinfo.
    // TODO: May want to ensure remaining VPTE flags are cleared in pagetable (including page 0)
    for (pa = 4096; pa < (vm_addr)0x00077000; pa += PAGE_SIZE) {
        if (pageframe_table[pa/PAGE_SIZE].flags & PGF_USER) {
            vpte_base = (struct PmapVPTE *)((uint8 *)pt + VPTE_TABLE_OFFS);                        
            pte_idx = (pa & L2_ADDR_BITS) >> L2_IDX_SHIFT;            
            vpte_base[pte_idx].flags = 0;
            vpte_base[pte_idx].link.prev = NULL;
            vpte_base[pte_idx].link.next = NULL;
            LIST_ADD_HEAD (&pageframe_table[pa/PAGE_SIZE].pmap_pageframe.vpte_list, &vpte_base[pte_idx], link);
        }        
    }
}


/*!
*/
void CoalesceFreePageframes (void)
{
    vm_addr pa;
    vm_addr pa2;
    int slab_free_page_cnt;
    
    LIST_INIT (&free_4k_pf_list);
    LIST_INIT (&free_16k_pf_list);
    LIST_INIT (&free_64k_pf_list);
    
    for (pa=0; pa + 0x10000 < mem_size; pa += 0x10000) {
        slab_free_page_cnt = 0;
                
        for (pa2= pa; pa2 < pa + 0x10000; pa2 += PAGE_SIZE) {
            if (pageframe_table[pa2/PAGE_SIZE].flags == 0) {
                slab_free_page_cnt ++;                
            }
        }
        
        if (slab_free_page_cnt * PAGE_SIZE == 0x10000) {
            pageframe_table[pa/PAGE_SIZE].size = 0x10000;
            LIST_ADD_TAIL (&free_64k_pf_list, &pageframe_table[pa/PAGE_SIZE], link);
        }        
        else {            
            for (pa2 = pa; pa2 < pa + 0x10000; pa2 += PAGE_SIZE) {
                if (pageframe_table[pa2/PAGE_SIZE].flags == 0) {
                    pageframe_table[pa2/PAGE_SIZE].size = PAGE_SIZE;
                    LIST_ADD_TAIL (&free_4k_pf_list, &pageframe_table[pa2/PAGE_SIZE], link);
                }
            }
        }
    }
    
    if (pa < mem_size) {
        for (pa2 = pa; pa2 < mem_size; pa2 += PAGE_SIZE) {
            if (pageframe_table[pa2/PAGE_SIZE].flags == 0) {
                pageframe_table[pa2/PAGE_SIZE].size = PAGE_SIZE;
                LIST_ADD_TAIL (&free_4k_pf_list, &pageframe_table[pa2/PAGE_SIZE], link);
            }
        }
    }
}


/*!
    Eventually a file cache will be implemented in the remaining area above where
    the IO areas are mapped.  Not important right now.
*/
void InitPageCache (void)
{
    int t;

    LIST_INIT (&free_pagecache_list);

    for (t=0; t< NPAGECACHE_HASH; t++) {
        LIST_INIT (&pagecache_hash[t]);
    }

    for (t=0; t < max_pagecache; t++) {
        LIST_ADD_TAIL (&free_pagecache_list, &pagecache_table[t], link);
    }
}   







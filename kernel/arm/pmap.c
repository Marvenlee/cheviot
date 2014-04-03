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

/*
 * ARM-Specific memory management code.  Deals with converting high-level
 * Segment and MemArea abstractions into page-tables.
 */

#include <kernel/types.h>
#include <kernel/vm.h>
#include <kernel/arm/arm.h>
#include <kernel/dbg.h>
#include <kernel/utility.h>
#include <kernel/proc.h>
#include <kernel/error.h>
#include <kernel/globals.h>




/*
 * Checks if the flags argument of VirtualAlloc() and VirtualAllocPhys() is
 * valid and supported.  Returns 0 on success, -1 otherwise.
 */

int PmapSupportsCachePolicy (bits32_t flags)
{
    return 0;
}




/*
 * Converts flags provided by VirtualAlloc() and VirtualAllocPhys() and stored
 * in the MemArea into ARM-specific Page Table Entry flags.
 */

uint32 PmapFlagsToPTE (bits32_t flags)
{
    uint32 pa_bits;
    
    if ((flags & (PROT_WRITE | MAP_VERSION)) == PROT_WRITE)
        pa_bits = L2_AP(AP_U | AP_W) | L2_TYPE_S;
    else
        pa_bits = L2_AP(AP_U) | L2_TYPE_S;
     
    if ((flags & MEM_MASK) == MEM_ALLOC)
    {
        pa_bits |= L2_B | L2_C;
    }
        
    return pa_bits;
}







/*
 * Initializes the CPU dependent part of the virtual memory management that
 * converts the higher level MemArea and Segment abstractions into page tables.
 *
 * On ARM CPUs each Process has 16 1k page tables that are recycled based on
 * the least recently used.  These 16 entries are swapped into a single 16k
 * page table on task switches.
 */

int PmapInit (struct Pmap *pmap)
{
    int t;
    struct PmapMem *pmap_mem;
    
    KASSERT (pmap != NULL);
    
    pmap_mem = LIST_HEAD (&free_pagetable_list);
        
    KASSERT (pmap_mem != NULL);
    
    LIST_REM_HEAD (&free_pagetable_list, free_link);
    
    pmap->addr = (uint32 *)pmap_mem;
    pmap->lru = 0;
    
    for (t=0; t< NPDE; t++)
    {
        pmap->pde[t].idx = -1;
        pmap->pde[t].val = 0;
    }

    PmapFlushTLBs();
    return 0;
}   
    



/*
 * Frees and CPU dependent virtual memory management structures.
 */

void PmapDestroy (struct Pmap *pmap)
{
    struct PmapMem *pmap_mem;
 
    KASSERT (pmap != NULL);
    
    pmap_mem = (struct PmapMem *)pmap->addr;
    KASSERT (pmap_mem != NULL);    
    LIST_ADD_HEAD (&free_pagetable_list, pmap_mem, free_link);
    
    PmapFlushTLBs();
}




/*
 * Converts the higher level VM description of MemAreas and Segments
 * into page tables.  Called whenever a memory map is changed through
 * the VirtualXXX() calls, message passing or a page fault occurs.
 * Updates part of a process's page table with new page table entries
 * based on the specified address and the MemArea that contains it.
 * An optimization is to set several page table entries at once.
 */

void PmapEnterRegion (struct Pmap *pmap, struct MemArea *ma, vm_addr addr)
{
    uint32 *pt;
    int pde_idx, pte_idx;
    uint32 pa_bits;
    vm_addr va;
    vm_addr pa;
    vm_addr ceiling;
    

    KASSERT ((addr & 0x00000fff) == 0);
    KASSERT (ma->base <= addr && addr < ma->ceiling);
    KASSERT (pmap != NULL);
    KASSERT (ma != NULL);
    KASSERT (pagedirectory != NULL);
    KASSERT (addr >= 0x00800000);
    KASSERT ((ma->flags & MEM_MASK) == MEM_ALLOC || (ma->flags & MEM_MASK) == MEM_PHYS);
    
    
    pde_idx = (addr & L1_ADDR_BITS) >> L1_IDX_SHIFT;
        
    if ((pagedirectory[pde_idx] & L1_TYPE_MASK) == L1_TYPE_INV)
    {
        KASSERT (pmap->pde[pmap->lru].idx >= -1 && pmap->pde[pmap->lru].idx < 4096);
        
        if (pmap->pde[pmap->lru].idx != -1)
        {
            pagedirectory[pmap->pde[pmap->lru].idx] = L1_TYPE_INV;
        }

        pt = (uint32 *)((uint8*)pmap->addr + pmap->lru * L2_TABLE_SIZE);

        KASSERT ((vm_addr)pt >= 0x00404000 && (vm_addr)pt < 0x00800000);

        pmap->pde[pmap->lru].val = (uint32)pt | L1_TYPE_C;
        pmap->pde[pmap->lru].idx = pde_idx;

        pagedirectory[pde_idx] = pmap->pde[pmap->lru].val;
                
        pmap->lru++;
        pmap->lru %= NPDE;
                
        MemSet (pt, 0, L2_TABLE_SIZE);
    }

    pt = (uint32 *)(pagedirectory[pde_idx] & L1_C_ADDR_MASK);

    KASSERT ((vm_addr)pt >= 0x00404000 && (vm_addr)pt < 0x00800000);

    pte_idx = (addr & L2_ADDR_BITS) >> L2_IDX_SHIFT;
    
    pa = ma->physical_addr + (addr - ma->base);
    pa_bits = PmapFlagsToPTE (ma->flags);

    ceiling = ALIGN_UP (addr + PAGE_SIZE, 0x00100000);
    
    if (ceiling >= ma->ceiling)
        ceiling = ma->ceiling;
    
    if ((addr + 0x00010000) < ceiling)
        ceiling = addr + 0x00010000;

    for (va = addr; va < ceiling; va += PAGE_SIZE, pa += PAGE_SIZE, pte_idx++)
        pt[pte_idx] = pa | pa_bits;
        
    PmapFlushTLBs();
}





/*
 * Unmaps and removes all of the page table entries for a range of memory
 * specified by a MemArea structure.
 */

void PmapRemoveRegion (struct Pmap *pmap, struct MemArea *ma)
{
    uint32 *pt;
    uint32 pde_idx, pte_idx;
    vm_addr va;

    KASSERT (pmap != NULL);
    KASSERT (ma != NULL);
    
    for (va = ma->base; va < ma->ceiling; va += PAGE_SIZE)
    {
        pde_idx = (va & L1_ADDR_BITS) >> L1_IDX_SHIFT;
        pt = (uint32 *)(pagedirectory[pde_idx] & L1_C_ADDR_MASK);
        pte_idx = (va & L2_ADDR_BITS) >> L2_IDX_SHIFT;
        pt[pte_idx] = L2_TYPE_INV;
    }
    
    PmapFlushTLBs();
}




/*
 * Flushes the CPU's TLBs once page tables are updated.
 */

void PmapFlushTLBs (void)
{
    InvalidateTLB();
}





/*
 * Switches the address space from the current process to the next process.
 * Each process uses only 16 1k page tables and these are swapped over
 * in a single page directory shared between all processes.
 */

void PmapSwitch (struct Pmap *next_pmap, struct Pmap *current_pmap)
{
    int t;

    KASSERT (next_pmap != NULL && current_pmap != NULL);
    
    for (t=0; t< NPDE; t++)
    {
        if (current_pmap->pde[t].idx == -1)
            continue;
            
        pagedirectory[current_pmap->pde[t].idx] = L1_TYPE_INV;
    }
    
    for (t=0; t< NPDE; t++)
    {
        if (next_pmap->pde[t].idx == -1)
            continue;
            
        pagedirectory[next_pmap->pde[t].idx] = next_pmap->pde[t].val;
    }
    
    PmapFlushTLBs();
}







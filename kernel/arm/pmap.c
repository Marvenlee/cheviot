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
 * VirtSeg and PhysSeg segment tables into CPU page tables.
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
 *
 */

void PmapPageFault (void)
{
    struct Process *current;
    struct Segment *seg;
    vm_addr addr;
    bits32_t access;
    
    current = GetCurrentProcess();
        
    KASSERT (current != NULL);
    KASSERT (!(current->flags & PROCF_DAEMON));
    
    addr = ALIGN_DOWN (current->task_state.fault_addr, PAGE_SIZE);
    access = current->task_state.fault_access;
    
    seg = SegmentFind (addr);
    
    if (addr < user_base || addr >= user_ceiling
        || seg == NULL || seg->owner != current
        || MEM_TYPE(seg->flags) == MEM_FREE
        || MEM_TYPE(seg->flags) == MEM_RESERVED
        || (access & seg->flags) != access)
    {
        current->task_state.flags |= TSF_EXCEPTION;
        DoExit(EXIT_FATAL);
    }

    DisablePreemption();
    
    if (seg->flags & SEG_COMPACT)
        Sleep (&compact_rendez);
            
    if (seg->flags & MAP_VERSION && access & PROT_WRITE)
    {
        seg->flags &= ~MAP_VERSION;
        seg->segment_id = next_unique_segment_id ++;
    }
    
    PmapEnterRegion(seg, addr);
    
    current->task_state.flags &= ~TSF_PAGEFAULT;
    KLog ("Returning from fault");
}




/*
 * Checks if the flags argument of VirtualAlloc() and VirtualAllocPhys() is
 * valid and supported.  Returns 0 on success, -1 otherwise.
 */

int PmapSupportsCachePolicy (bits32_t flags)
{
    return 0;
}



vm_size PmapAlignmentSize (ssize_t size)
{
    if (size >= 0x100000)
        return 0x100000;
    else if (size >= 0x10000)
        return 0x10000;
    else
        return 0;
}






/*
 * Initializes the CPU dependent part of the virtual memory management that
 * converts the higher level MemArea and Segment abstractions into page tables.
 *
 * On ARM CPUs each Process has 16 1k page tables that are recycled based on
 * the least recently used.  These 16 entries are swapped into a single 16k
 * page table on task switches.
 */

int PmapInit (struct Process *proc)
{
    proc->pmap = NULL;
    
    PmapFlushTLBs();
    return 0;
}   
    



/*
 * Frees CPU dependent virtual memory management structures.
 */

void PmapDestroy (struct Process *proc)
{
    struct Pmap *pmap;
    
    pmap = proc->pmap;

    if (pmap != NULL)
    {
        LIST_ADD_TAIL (&pmap_lru_list, pmap, lru_link);
        proc->pmap = NULL;
    }
        
    PmapFlushTLBs();
}




/*
 * Maps a segment into the address space pointed to by pmap.
 *
 * Converts the higher level VM description of MemAreas and Segments
 * into page tables.  Called whenever a memory map is changed through
 * the allocation, message passing or a page fault occurs.
 * Updates part of a process's page table with new page table entries
 * based on the specified address and the MemArea that contains it.
 * An optimization is to set several page table entries at once.
 *
 * Maps upto 256k on each call to PmapEnterRegion(), using the addr
 * argument as a starting point.  Supports ARM's large 64k page size
 * if the virtual and physical address of a segment are 64k aligned.
 *
 *
 * FIXME: Add support for 1MB ARM sections.
 *
 * AWOOGA FIXME: PmapEnter assumes we are updating pmap of current process
 * and modifying the page directory.
 */

void PmapEnterRegion (struct Segment *seg, vm_addr addr)
{
    uint32 *pt;
    int pde_idx, pte_idx;
    uint32 pa_bits;
    vm_addr va;
    vm_addr pa;
    vm_addr ceiling;
    int t;
    struct Pmap *pmap;
    
    
    KASSERT (seg != NULL);
    KASSERT ((addr & 0x00000fff) == 0);
    KASSERT (addr >= 0x00800000);
    KASSERT (seg->base <= addr && addr < seg->base + seg->size);
    KASSERT ((seg->flags & MEM_MASK) == MEM_ALLOC || (seg->flags & MEM_MASK) == MEM_PHYS);

    if (seg->owner == NULL || seg->owner->pmap == NULL)
        return;

    pmap = seg->owner->pmap;
    
        
    pde_idx = (addr & L1_ADDR_BITS) >> L1_IDX_SHIFT;
    
    pt = PmapGetPageTable (pmap, pde_idx);
    
    
    if ((seg->flags & SEG_KERNEL) == 0)
        pa_bits = L2_AP(AP_W);
    else if ((seg->flags & (PROT_WRITE | MAP_VERSION)) == PROT_WRITE)
        pa_bits = L2_AP(AP_U | AP_W);
    else
        pa_bits = L2_AP(AP_U);
     
    if ((seg->flags & MEM_MASK) == MEM_ALLOC)
        pa_bits |= L2_B | L2_C;
    
    if ((seg->base & 0x0ffff) == 0 && (seg->physical_addr & 0x0ffff) == 0)
    {
        addr = ALIGN_DOWN (addr, 0x10000);

        pte_idx = (addr & L2_ADDR_BITS) >> L2_IDX_SHIFT;
        
        pa = seg->physical_addr + (addr - seg->base);
        pa_bits |= L2_TYPE_L;
        
        ceiling = ALIGN_UP (addr + PAGE_SIZE, 0x00100000);
        
        if (ceiling >= seg->base + seg->size)
            ceiling = seg->base + seg->size;
        
        if ((addr + 0x00040000) < ceiling)
            ceiling = addr + 0x00040000;
        
        for (va = addr;
            va < ceiling && ceiling - va >= 0x10000;
            pa += 0x10000, pte_idx += 16)
        {
            for (t=0; t<16; t++)
               pt[pte_idx+t] = pa | pa_bits;
        }
        
        for (va = addr; va < ceiling; va += PAGE_SIZE, pa += PAGE_SIZE, pte_idx++)
            pt[pte_idx] = pa | pa_bits;
    }
    else
    {
        pte_idx = (addr & L2_ADDR_BITS) >> L2_IDX_SHIFT;
    
        pa = seg->physical_addr + (addr - seg->base);
        pa_bits |= L2_TYPE_S;
        
        ceiling = ALIGN_UP (addr + PAGE_SIZE, 0x00100000);
        
        if (ceiling >= seg->base + seg->size)
            ceiling = seg->base + seg->size;
        
        if ((addr + 0x00040000) < ceiling)
            ceiling = addr + 0x00040000;
    
        for (va = addr; va < ceiling; va += PAGE_SIZE, pa += PAGE_SIZE, pte_idx++)
            pt[pte_idx] = pa | pa_bits;
    }    
        
    PmapFlushTLBs();
}




/*
 *
 */
 
uint32 *PmapGetPageTable (struct Pmap *pmap, int pde_idx)
{
    uint32 *pt;
    int t;

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

        for (t=0; t<256; t++)
            *(pt + t) = L2_TYPE_INV;
    }
    else
    {
        pt = (uint32 *)(pagedirectory[pde_idx] & L1_C_ADDR_MASK);
        KASSERT ((vm_addr)pt >= 0x00404000 && (vm_addr)pt < 0x00800000);
    }

    return pt;
}




/*
 * Unmaps a segment from the address space pointed to by pmap. 
 * Removes page table entries of the first and last page tables that
 * partially intersect with the segment.  Checks the pmap's 16 page tables
 * to see if they are eclipsed and removes the page directory entry instead. 
 *
 *
 * AWOOGA FIXME:  May need some form of interprocessor interrupt for pagetable
 * teardown.
 */

void PmapRemoveRegion (struct Segment *seg)
{
    uint32 *pt;
    uint32 pde_idx, pte_idx;
    vm_addr va;
    vm_addr first_pagetable_ceiling;
    vm_addr last_pagetable_base;
    vm_addr temp_ceiling;
    int t; 
    struct Pmap *pmap;
    
    
    KASSERT (seg != NULL);
    KASSERT ((seg->flags & MEM_MASK) == MEM_ALLOC || (seg->flags & MEM_MASK) == MEM_PHYS);

    if (seg->owner == NULL || seg->owner->pmap == NULL)
        return;
    
    pmap = seg->owner->pmap;
    
    first_pagetable_ceiling = ALIGN_UP (seg->base, 0x100000);
    last_pagetable_base = ALIGN_DOWN (seg->base + seg->size, 0x100000);
    
    va = seg->base;
    
    if (seg->base + seg->size < first_pagetable_ceiling)
        temp_ceiling = seg->base + seg->size;
    else
        temp_ceiling = first_pagetable_ceiling;    
    
    pde_idx = (va & L1_ADDR_BITS) >> L1_IDX_SHIFT;    
    pt = (uint32 *)(pagedirectory[pde_idx] & L1_C_ADDR_MASK);
    pte_idx = (va & L2_ADDR_BITS) >> L2_IDX_SHIFT;
            
    while (va < temp_ceiling)
    {
        pt[pte_idx] = L2_TYPE_INV;
        va += PAGE_SIZE;
        pte_idx ++;
    }
    
    if (va == seg->base + seg->size)
        return;
    
    if ((seg->base + seg->size - va) >= 0x100000)
    {
        for (t=0; t<NPDE; t++)
        {   
            va = pmap->pde[t].idx * 0x100000;
            
            if (first_pagetable_ceiling <= va && va < last_pagetable_base)
            {
                pagedirectory[pmap->pde[t].idx] = L1_TYPE_INV;
                pmap->pde[t].idx = -1;
                pmap->pde[t].val = L1_TYPE_INV;
            }
        }
    }    
    
    va = last_pagetable_base;
    
    if (va == seg->base + seg->size)
        return;
    
    pde_idx = (va & L1_ADDR_BITS) >> L1_IDX_SHIFT;    
    pt = (uint32 *)(pagedirectory[pde_idx] & L1_C_ADDR_MASK);
    pte_idx = (va & L2_ADDR_BITS) >> L2_IDX_SHIFT;
    
    while (va < seg->base + seg->size)
    {
        pt[pte_idx] = L2_TYPE_INV;
        va += PAGE_SIZE;
        pte_idx ++;
    }
        
    PmapFlushTLBs();
}




/*
 *
 */
 
void PmapCachePreDMA (struct Segment *seg)
{
    FlushCaches();
}




/*
 *
 */

void PmapCachePostDMA (struct Segment *seg)
{
    FlushCaches();
}




/*
 * Flushes the CPU's TLBs once page tables are updated.
 */

void PmapFlushTLBs (void)
{
    InvalidateTLB();
}




/*
 *
 */
 
void PmapInvalidateAll (void)
{
    int t;
    
    
    for (t=0; t < max_process; t++)
    {
        // FIXME: For each alive process,  set the 16 pmap entries to -1
    }
    
    InvalidateTLB();
}




/*
 * Switches the address space from the current process to the next process.
 * Each process uses only 16 1k page tables and these are swapped over
 * in a single page directory shared between all processes.
 */

void PmapSwitch (struct Process *next, struct Process *current)
{
    int t;
 
    if (current->pmap != NULL)
    {
        for (t=0; t< NPDE; t++)
        {
            if (current->pmap->pde[t].idx != -1)
                pagedirectory[current->pmap->pde[t].idx] = L1_TYPE_INV;
        }
    }
    
    LIST_REM_ENTRY (&pmap_lru_list, current->pmap, lru_link);
    LIST_ADD_TAIL (&pmap_lru_list, current->pmap, lru_link);
    
    if (next->pmap == NULL)
    {
        next->pmap = LIST_TAIL (&pmap_lru_list);
        LIST_REM_TAIL (&pmap_lru_list, lru_link);
        LIST_ADD_HEAD (&pmap_lru_list, next->pmap, lru_link);
    
        for (t=0; t < NPDE; t++)
        {
            next->pmap->pde[t].idx = -1;
            next->pmap->pde[t].val = L1_TYPE_INV;            
        } 
    }
    else
    {
        for (t=0; t< NPDE; t++)
        {
            if (next->pmap->pde[t].idx != -1)
                pagedirectory[next->pmap->pde[t].idx] = next->pmap->pde[t].val;
        }
    }
        
    // Reload Page Directory with physically mapped pagedir. if PROCF_DAEMON;
    
    if (next->flags & PROCF_DAEMON)
        SetPageDirectory (phys_pagedirectory);
    else
        SetPageDirectory (pagedirectory);
}







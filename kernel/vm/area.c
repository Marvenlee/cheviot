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
 * Virtual address space management.  The memarea structure describes a region
 * of virtual address space. There is a corresponding segment_table that
 * manages physical memory.
 * 
 * SUGGESTION: May want to double buffer segment and memarea tables so that
 * we update the alternate table on allocation and coalescing. This will enable
 * preemption to be enabled during searching and coalescing.  Preemption will
 * only be disabled to update the pointers to the current tables to use.
 */
 
#include <kernel/types.h>
#include <kernel/vm.h>
#include <kernel/lists.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/utility.h>
#include <kernel/globals.h>
#include <limits.h>



/*
 * Coalesces free virtual memory areas and compacts the vseg array.
 * Called periodically by the kernel's VM daemon.
 */
 
SYSCALL int CoalesceVirtualMem (void)
{
    int t, s;
    struct Process *current;
    

    current = GetCurrentProcess();
    
    if (!(current->flags & PROCF_DAEMON))
        return privilegeErr;
    
    DisablePreemption();
    
    for (t = 0, s = 0; t < vseg_cnt; t++)
    {
        if (s > 0 && (MEM_TYPE(vseg_table[s-1].flags) == MEM_FREE
                                 && MEM_TYPE(vseg_table[t].flags) == MEM_FREE))
        {
            vseg_table[s-1].ceiling = vseg_table[t].ceiling;
        }
        else
        {
            vseg_table[s].base          = vseg_table[t].base;
            vseg_table[s].ceiling       = vseg_table[t].ceiling;
            vseg_table[s].physical_addr = vseg_table[t].physical_addr;
            vseg_table[s].flags         = vseg_table[t].flags;
            vseg_table[s].owner         = vseg_table[t].owner;
            vseg_table[s].parcel        = vseg_table[t].parcel;
            vseg_table[s].busy          = vseg_table[t].busy;
            vseg_table[s].wired         = vseg_table[t].wired;
            
            if ((vseg_table[s].parcel) != NULL)
                vseg_table[s].parcel->content.msg = vseg_table[s].base;

            s++;
        }
    }

    vseg_cnt = s;
    return 0;
}




/*
 * VirtualSegmentCreate();
 */

struct VirtualSegment *VirtualSegmentCreate (vm_offset addr, vm_size size, bits32_t flags)
{
    struct VirtualSegment *vs;
    vm_addr base, ceiling;
    int t;

    KASSERT (vseg_cnt < (max_vseg - 3));
    
    if (flags & MAP_FIXED)
    {   
        if ((vs = VirtualSegmentFind(addr)) == NULL)
        {
            return NULL;
        }
            
        if (vs->ceiling - addr < size || MEM_TYPE(vs->flags) != MEM_FREE)
        {
            return NULL;
        }
    }
    else if ((vs = VirtualSegmentBestFit (size, flags, &addr)) == NULL)
    {
        return NULL;
    }
    
    KASSERT (MEM_TYPE(vs->flags) == MEM_FREE);
    KASSERT (vs->base <= addr && addr+size <= vs->ceiling);
    
    t = vs - vseg_table;

    if (vs->base == addr && vs->ceiling == addr + size)
    {
        /* Perfect fit, initialize region */
        
        vs->base = addr;
        vs->ceiling = addr + size;
        vs->flags = flags;
        vs->owner = NULL;
        vs->parcel = NULL;
    }
    else if (vs->base < addr && vs->ceiling > addr + size)
    {
        /* In the middle, between two parts */
    
        base = vs->base;
        ceiling = vs->ceiling;
    
        VirtualSegmentInsert (t, 2);
                
        vs->base = base;
        vs->ceiling = addr;
        vs->flags = MEM_FREE;
        vs++;
        
        vs->base = addr;
        vs->ceiling = addr + size;
        vs->flags = flags;
        vs->owner = NULL;
        vs->parcel = NULL;
        vs++;
        
        vs->base = addr + size;
        vs->ceiling = ceiling;
        vs->flags = MEM_FREE;
        vs --;      // Return the middle vs
        
    }
    else if (vs->base == addr && vs->ceiling > addr + size)
    {
        /* Starts at bottom of area */
    
        base = vs->base;
        ceiling = vs->ceiling;
    
        VirtualSegmentInsert (t, 1);
        
        vs->base = addr;
        vs->ceiling = addr + size;
        vs->flags = flags;
        vs->owner = NULL;
        vs->parcel = NULL;
        vs++;
        
        vs->base = addr + size;
        vs->ceiling = ceiling;
        vs->flags = MEM_FREE;
        vs --;      // Return the first vs
    }
    else
    {
        /* Starts at top of area */
        
        base = vs->base;
        ceiling = vs->ceiling;
    
        VirtualSegmentInsert (t, 1);
        
        vs->base = base;
        vs->ceiling = addr;
        vs->flags = MEM_FREE;
        
        vs++;
        vs->base = addr;
        vs->ceiling = addr + size;
        vs->flags = flags;
        vs->owner = NULL;
        vs->parcel = NULL;
        
        // Return the second vs
    }
    
    
    return vs;
}




/*
 * VirtualSegmentDelete();
 */
 
void VirtualSegmentDelete (struct VirtualSegment *vs)
{
    KASSERT (MEM_TYPE(vs->flags) != MEM_FREE);
        
    vs->flags = (vs->flags & ~MEM_MASK) | MEM_FREE;
}




/*
 * VirtualSegmentInsert();
 *


 */
 
void VirtualSegmentInsert (int index, int cnt)
{
    int t;
    
    for (t = vseg_cnt - 1 + cnt; t >= (index + cnt); t--)
    {
        vseg_table[t].base          = vseg_table[t-cnt].base;
        vseg_table[t].ceiling       = vseg_table[t-cnt].ceiling;
        vseg_table[t].physical_addr = vseg_table[t-cnt].physical_addr;
        vseg_table[t].flags         = vseg_table[t-cnt].flags;
        vseg_table[t].owner         = vseg_table[t-cnt].owner;
        vseg_table[t].parcel        = vseg_table[t-cnt].parcel;
        vseg_table[t].busy          = vseg_table[t-cnt].busy;
        vseg_table[t].wired         = vseg_table[t-cnt].wired;
            
        if ((vseg_table[t].parcel) != NULL)
              vseg_table[t].parcel->content.msg = vseg_table[t].base;
    }
    
    vseg_cnt += cnt;
}




/*
 * Looks up and returns a VirtualSegment for a given virtual address.
 */

struct VirtualSegment *VirtualSegmentFind (vm_addr addr)
{
    int low = 0;
    int high = vseg_cnt - 1;
    int mid;    
        
        
    while (low <= high)
    {
        mid = low + ((high - low) / 2);
        
        if (addr >= vseg_table[mid].ceiling)
            low = mid + 1;
        else if (addr < vseg_table[mid].base)
            high = mid - 1;
        else
            return &vseg_table[mid];
    }

    return NULL;
}





/*
 * Finds the best fitting VirtualSegment for the given size and flags.
 *
 * If the best fitting segment is large enough, this will attempt to align the
 * allocation up to a LARGE_PAGE_SIZE (usually 64k) so that the page table code
 * can attempt to use larger page sizes.  This requires physical mem to also be
 * aligned on a large page table boundary.
 */

struct VirtualSegment *VirtualSegmentBestFit (vm_size size, uint32 flags, vm_addr *ret_addr)
{
    int i;
    vm_size vs_size;
    vm_size best_size = VM_USER_CEILING - VM_USER_BASE;
    int best_idx = -1;
    vm_size new_size;
    vm_addr new_base;
    
    
    for (i = 0; i < vseg_cnt; i++)
    {
        if (MEM_TYPE(vseg_table[i].flags) == MEM_FREE)
        {
            vs_size = vseg_table[i].ceiling - vseg_table[i].base;

            if (vs_size > size && vs_size < best_size)
            {
                best_size = vs_size;
                best_idx = i;
            }
            else if (vs_size == size)
            {
                *ret_addr = vseg_table[i].base;
                return &vseg_table[i];
            }
        }           
    }
    
    if (best_idx == -1)
        return NULL;


    
    if (best_size >= LARGE_PAGE_SIZE)
    {
        new_base = ALIGN_UP (vseg_table[i].base, LARGE_PAGE_SIZE);
        
        if (new_base < vseg_table[i].ceiling)
        {
            new_size = vseg_table[i].ceiling - new_base;
        
            if (new_size >= size)
            {
                *ret_addr = new_base;
                return &vseg_table[best_idx];
            }
        }
    }

    *ret_addr = vseg_table[i].base;
    return &vseg_table[best_idx];
}




/*
 * Used by Spawn() to lookup multiple VirtualSegment structures
 * Initializes the vseg array with pointers to the found areas.
 * Ensures each VirtualSegment is unique otherwise returns an error.
 *
 * OPTIMIZE: For sizes greater than 64k, w should try to find a suitable
 * segment where we can create an allocation that is 64k aligned to make
 * use of a CPU's larger TLB sizes.  Same applies for physical allocation
 * and compaction.
 *
 */
 
int VirtualSegmentFindMultiple (struct VirtualSegment **vseg, void **mem_ptrs, int cnt)
{
    struct VirtualSegment *vs;
    struct Process *current;
    int t, u;
    
    
    current = GetCurrentProcess();

    for (t=0; t< cnt; t++)
    {
        vs = VirtualSegmentFind ((vm_addr)mem_ptrs[t]);
        
        if (vs == NULL || vs->owner != current || (vs->flags & MEM_MASK) != MEM_ALLOC)
            return memoryErr;
            
        if ((vs->ceiling - vs->base) > INT_MAX)
            return memoryErr;   
        
        if (vs->wired == TRUE)
            return memoryErr;
        
        for (u=0; u<t; u++)
        {
            if (vs == vseg[u])
                return paramErr;
        }
        
        vseg[t] = vs;
    }
    
    return 0;
}   

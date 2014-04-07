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
 * Coalesces free virtual memory areas and compacts the virtseg array.
 */
 
SYSCALL int CoalesceVirtualMem (void)
{
    int t, s;
    struct Process *current;
    

    current = GetCurrentProcess();
    
    if (!(current->flags & PROCF_EXECUTIVE))
        return privilegeErr;
    
    DisablePreemption();
    
    for (t = 0, s = 0; t < virtseg_cnt; t++)
    {
        if (s > 0 && (MEM_TYPE(virtseg_table[s-1].flags) == MEM_FREE
                                 && MEM_TYPE(virtseg_table[t].flags) == MEM_FREE))
        {
            virtseg_table[s-1].ceiling = virtseg_table[t].ceiling;
        }
        else
        {
            virtseg_table[s].base          = virtseg_table[t].base;
            virtseg_table[s].ceiling       = virtseg_table[t].ceiling;
            virtseg_table[s].physical_addr = virtseg_table[t].physical_addr;
            virtseg_table[s].flags         = virtseg_table[t].flags;
            virtseg_table[s].owner         = virtseg_table[t].owner;
            virtseg_table[s].parcel        = virtseg_table[t].parcel;
            virtseg_table[s].busy          = virtseg_table[t].busy;
            virtseg_table[s].wired         = virtseg_table[t].wired;
            
            if ((virtseg_table[s].parcel) != NULL)
                virtseg_table[s].parcel->content.virtualsegment = &virtseg_table[s];

            s++;
        }
    }

    virtseg_cnt = s;
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

    KASSERT (virtseg_cnt < (max_virtseg - 3));
    
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
    
    t = vs - virtseg_table;

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
    
    for (t = virtseg_cnt - 1 + cnt; t >= (index + cnt); t--)
    {
        virtseg_table[t].base          = virtseg_table[t-cnt].base;
        virtseg_table[t].ceiling       = virtseg_table[t-cnt].ceiling;
        virtseg_table[t].physical_addr = virtseg_table[t-cnt].physical_addr;
        virtseg_table[t].flags         = virtseg_table[t-cnt].flags;
        virtseg_table[t].owner         = virtseg_table[t-cnt].owner;
        virtseg_table[t].parcel        = virtseg_table[t-cnt].parcel;
        virtseg_table[t].busy          = virtseg_table[t-cnt].busy;
        virtseg_table[t].wired         = virtseg_table[t-cnt].wired;
            
        if ((virtseg_table[t].parcel) != NULL)
              virtseg_table[t].parcel->content.virtualsegment = &virtseg_table[t];
    }
    
    virtseg_cnt += cnt;
}




/*
 * Looks up and returns a VirtualSegment for a given virtual address.
 */

struct VirtualSegment *VirtualSegmentFind (vm_addr addr)
{
    int low = 0;
    int high = virtseg_cnt - 1;
    int mid;    
        
        
    while (low <= high)
    {
        mid = low + ((high - low) / 2);
        
        if (addr >= virtseg_table[mid].ceiling)
            low = mid + 1;
        else if (addr < virtseg_table[mid].base)
            high = mid - 1;
        else
            return &virtseg_table[mid];
    }

    return NULL;
}





/*
 * Finds the best fitting VirtualSegment for the given size and flags.
 */

struct VirtualSegment *VirtualSegmentBestFit (vm_size size, uint32 flags, vm_addr *ret_addr)
{
    int i;
    vm_size vs_size;
    vm_size best_size = VM_USER_CEILING - VM_USER_BASE;
    int best_idx = -1;
    

    for (i = 0; i < virtseg_cnt; i++)
    {
        if (MEM_TYPE(virtseg_table[i].flags) == MEM_FREE)
        {
            vs_size = virtseg_table[i].ceiling - virtseg_table[i].base;
        
            if (vs_size > size && vs_size < best_size)
            {
                best_size = vs_size;
                best_idx = i;
            }
            else if (vs_size == size)
            {
                *ret_addr = virtseg_table[i].base;
                return &virtseg_table[i];
            }
        }           
    }
    
    if (best_idx == -1)
        return NULL;

    *ret_addr = virtseg_table[best_idx].base;
    return &virtseg_table[best_idx];
}




/*
 * Used by Spawn() to lookup multiple VirtualSegment structures
 * Initializes the virtseg array with pointers to the found areas.
 * Ensures each VirtualSegment is unique otherwise returns an error.
 *
 * OPTIMIZE: For sizes greater than 64k, w should try to find a suitable
 * segment where we can create an allocation that is 64k aligned to make
 * use of a CPU's larger TLB sizes.  Same applies for physical allocation
 * and compaction.
 *
 */
 
int VirtualSegmentFindMultiple (struct VirtualSegment **virtseg, void **mem_ptrs, int cnt)
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
            if (vs == virtseg[u])
                return paramErr;
        }
        
        virtseg[t] = vs;
    }
    
    return 0;
}   

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
 * Physical memory managment.  Manages physical memory using an array
 * of Segment structures.  Most functions are called by the Virtuslxyz()
 * functions in kernel/vm/vm.c.
 */
 
#include <kernel/types.h>
#include <kernel/lists.h>
#include <kernel/vm.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>




/*
 * Coalesces free physical memory segments but does not compact them.
 * Shrinks the size of the segment_table array that manages physical memory.
 */
 
SYSCALL int CoalescePhysicalMem (void)
{
    int t, s;
    struct Process *current;
    

    current = GetCurrentProcess();
    
    if (!(current->flags & PROCF_EXECUTIVE))
        return privilegeErr;

    DisablePreemption();

    for (t = 0, s = 0; t < segment_cnt; t++)
    {
        if (s > 0 && (segment_table[s-1].type == SEG_TYPE_FREE && segment_table[t].type == SEG_TYPE_FREE))
        {
            segment_table[s-1].ceiling = segment_table[t].ceiling;
        }
        else
        {
            segment_table[s].base        = segment_table[t].base;
            segment_table[s].ceiling       = segment_table[t].ceiling;
            segment_table[s].type        = segment_table[t].type;
            s++;
        }
    }

    segment_cnt = s;
    return 0;
}






/*
 * Compacts physical memory segments,  moves segments downwards in
 * physical memory unless they are locked by the WireMem() system
 * call.
 *
 * To be called by root process/Executive.  The actual compaction may be
 * implemented in a kernel thread that has physical memory mapped in its
 * address space.
 */

SYSCALL vm_addr CompactPhysicalMem (vm_addr addr)
{
    return addr;
}

















/*
 * Searches for and returns a suitably sized free physical memory segment of
 * the desired size and type. Inserts entries into the segment_table array
 * if needed
 */

struct Segment *SegmentCreate (vm_offset addr, vm_size size, int type)
{
    struct Segment *seg;
    vm_addr base, ceiling;
    bits32_t flags;
    int t;


    // FIXME: AWOOGA: Why set flags to zero??????

    flags = 0;
     
    KASSERT (segment_cnt < (max_segment - 3));   
    
    if ((seg = SegmentAlloc (size, flags, &addr)) == NULL)
        return NULL;
    
    
    KASSERT (seg->base <= addr && addr+size <= seg->ceiling);
    
    t = seg - segment_table;

    if (seg->base == addr && seg->ceiling == addr + size)
    {
        /* Perfect fit, initialize region */
        
        seg->base = addr;
        seg->ceiling = addr + size;
        seg->type = type;
    }
    else if (seg->base < addr && seg->ceiling > addr + size)
    {
        /* In the middle, between two parts */
    
        base = seg->base;
        ceiling = seg->ceiling;
    
        SegmentInsert (t, 2);
                
        seg->base = base;
        seg->ceiling = addr;
        seg->type = SEG_TYPE_FREE;
        seg++;
        
        seg->base = addr;
        seg->ceiling = addr + size;
        seg->type = type;
        seg++;
        
        seg->base = addr + size;
        seg->ceiling = ceiling;
        seg->type = SEG_TYPE_FREE;
        seg --;     // Return the middle seg
        
    }
    else if (seg->base == addr && seg->ceiling > addr + size)
    {
        /* Starts at bottom of area */
    
        base = seg->base;
        ceiling = seg->ceiling;
    
        SegmentInsert (t, 1);
        
        seg->base = addr;
        seg->ceiling = addr + size;
        seg->type = type;
        seg++;
        
        seg->base = addr + size;
        seg->ceiling = ceiling;
        seg->type = SEG_TYPE_FREE;
        seg --;         // Return the first seg
    }
    else
    {
        /* Starts at top of area */
        
        base = seg->base;
        ceiling = seg->ceiling;
    
        SegmentInsert (t, 1);
        
        seg->base = base;
        seg->ceiling = addr;
        seg->type = SEG_TYPE_FREE;
        
        seg++;
        seg->base = addr;
        seg->ceiling = addr + size;
        seg->type = type;
        
        // Return the second seg
    }
    
    
    return seg;
}




/*
 * Marks a segment as free.  Does not coalesce free segments.  Coalescing is
 * done by a seperate CoalescePhysicalMem() system call.
 */
 
void SegmentDelete (struct Segment *seg)
{
    seg->type = SEG_TYPE_FREE;
}




/*
 * Inserts 'cnt' empty segment_table entries after segment_table entry 'index' 
 * Used by SegmentCreate() if a perfectly sized segment could not be found.
 * This inserts free entries in the segment_table so that SegmentCreate() can
 * carve up a larger free segment into the desired size plus the leftovers.
 */
 
void SegmentInsert (int index, int cnt)
{
    int t;
    
    for (t = segment_cnt - 1 + cnt; t >= (index + cnt); t--)
    {
        segment_table[t].base = segment_table[t-cnt].base;
        segment_table[t].ceiling = segment_table[t-cnt].ceiling;
        segment_table[t].type = segment_table[t-cnt].type;
    }
    
    segment_cnt += cnt;
}




/*
 *
 */

struct Segment *SegmentFind (vm_addr addr)
{
    int low = 0;
    int high = segment_cnt - 1;
    int mid;    
    
    
    while (low <= high)
    {
        mid = low + ((high - low) / 2);
        
        if (addr >= segment_table[mid].ceiling)
            low = mid + 1;
        else if (addr < segment_table[mid].base)
            high = mid - 1;
        else
            return &segment_table[mid];
    }

    return NULL;
}




/*
 * Performs a best fit search for a segment of physical memory.
 * returns the segment and the physical address of where it segment
 * to be returned to the user should start.  This may be different
 * from the base address of the segment if alignment options
 * are added to the code.
 * SegmentCreate() then carves this segment up if it is larger than
 * the requested size.
 */

struct Segment *SegmentAlloc (vm_size size, uint32 flags, vm_addr *ret_addr)
{
    int i;
    vm_size seg_size;
    vm_size best_size = VM_USER_CEILING - VM_USER_BASE;
    int best_idx = -1;
    bool found = FALSE;
    
    
    
    // Might want to support NOX64 (same for compaction)

    for (i = 0; i < segment_cnt; i++)
    {
        if (segment_table[i].type != SEG_TYPE_FREE)
            continue;
        
        if (found == FALSE)
            found = TRUE;
    
        seg_size = segment_table[i].ceiling - segment_table[i].base;
    
        if (seg_size > size && seg_size < best_size)
        {
            best_size = seg_size;
            best_idx = i;
            
        }
        else if (seg_size == size)
        {
            *ret_addr = segment_table[i].base;
            return &segment_table[i];
        }
        
    }
    
    if (best_idx == -1)
        return NULL;

    *ret_addr = segment_table[best_idx].base;
    return &segment_table[best_idx];
}









































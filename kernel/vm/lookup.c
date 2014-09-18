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
 * Looks up and returns a Segment for a given virtual address.
 */

struct Segment *SegmentFind (vm_addr addr)
{
    int low = 0;
    int high = seg_cnt - 1;
    int mid;    
        
        
    while (low <= high)
    {
        mid = low + ((high - low) / 2);
        
        if (addr >= seg_table[mid].base + seg_table[mid].size)
            low = mid + 1;
        else if (addr < seg_table[mid].base)
            high = mid - 1;
        else
            return &seg_table[mid];
    }

    return NULL;
}




/*
 * Used by Spawn() to lookup multiple Segment structures
 * Initializes the vseg array with pointers to the found areas.
 * Ensures each Segment is unique otherwise returns an error.
 *
 * OPTIMIZE: For sizes greater than 64k, w should try to find a suitable
 * segment where we can create an allocation that is 64k aligned to make
 * use of a CPU's larger TLB sizes.  Same applies for physical allocation
 * and compaction.
 *
 */
 
int SegmentFindMultiple (struct Segment **segs, void **mem_ptrs, int cnt)
{
    struct Segment *seg;
    struct Process *current;
    int t, u;
    
    
    current = GetCurrentProcess();

    for (t=0; t< cnt; t++)
    {
        seg = SegmentFind ((vm_addr)mem_ptrs[t]);
        
        if (seg == NULL || seg->owner != current || (seg->flags & MEM_MASK) != MEM_ALLOC)
            return memoryErr;
        
        if (seg->flags & SEG_WIRED)
            return memoryErr;
        
        for (u=0; u<t; u++)
        {
            if (seg == segs[u])
                return paramErr;
        }
        
        segs[t] = seg;
    }
    
    return 0;
}   



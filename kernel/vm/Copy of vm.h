/*
 * Copyright 2014  Marven Gilhespie
 *
 * Licensed under the Apache License, segment_id 2.0 (the "License");
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
 * System calls for virtual memory management.
 */

#include <kernel/types.h>
#include <kernel/lists.h>
#include <kernel/arch.h>
#include <kernel/vm.h>
#include <kernel/proc.h>
#include <kernel/utility.h>
#include <kernel/error.h>
#include <kernel/dbg.h>
#include <kernel/globals.h>






/*
 * Allocates a clean area are virtual memory to the current process and
 * returns a pointer to it.  Returns NULL if unsuccessful.  Will sleep
 * for other tasks to free memory.
 *
 * The look up and creation of the vseg and pseg data structures
 * has preemption disabled.
 
 * This could be optimized to allow preemption during the lookup/best fit
 * of suitable sized virtual and physical segments.  If the virtseg and
 * physseg arrays need to grow then there are two options:
 *
 * 1) Disable preemption and resize arrays.
 * 2) Double-buffer, create a new array, with any insertions added, then
 *    only disable preemption to update a pointer to the new array.
 *    This may need a time limit after which preemption is disabled.
 */

SYSCALL vm_addr VirtualAlloc (vm_addr addr, ssize_t len, bits32_t flags)
{
    struct Segment *seg;

    
    addr = ALIGN_DOWN (addr, PAGE_SIZE);
    len = ALIGN_UP (len, PAGE_SIZE);

    if ((flags & CACHE_MASK) != CACHE_DEFAULT)
        return (vm_addr)NULL;
        
    flags = (flags & ~VM_SYSTEM_MASK) | MEM_ALLOC;
    
    DisablePreemption();
    
    if ((seg = SegmentAlloc ((vm_addr)NULL, len, flags)) == NULL)
        return (vm_addr)NULL;
    
    return seg->base;
}







/*
 * Maps an area of physical memory such as IO devie or framebuffer into the
 * address space of the calling process.
 */

SYSCALL vm_addr VirtualAllocPhys (vm_addr addr, ssize_t len, bits32_t flags, vm_addr paddr)
{
    struct Segment *seg;
    struct Process *current;


    current = GetCurrentProcess();

    addr = ALIGN_DOWN (addr, PAGE_SIZE);
    paddr = ALIGN_DOWN (paddr, PAGE_SIZE);
    len = ALIGN_UP (len, PAGE_SIZE);
    flags = (flags & ~VM_SYSTEM_MASK) | MEM_PHYS;

    if (!(current->flags & PROCF_ALLOW_IO))
        return (vm_addr)NULL;

    if (PmapSupportsCachePolicy (flags) == -1)
        return (vm_addr)NULL;

    DisablePreemption();

    if ((seg = SegmentAlloc (paddr, len, flags)) == NULL)
        return (vm_addr)NULL;
  
    return seg->base;
}




/*
 *
 */

SYSCALL int VirtualFree (vm_offset addr)
{
    struct Segment *seg;
    struct Process *current;
    
    current = GetCurrentProcess();

    addr = ALIGN_DOWN (addr, PAGE_SIZE);

    if ((seg = SegmentFind (addr)) == NULL || seg->owner != current)
        return memoryErr;
    
    DisablePreemption();
    
    PmapRemoveRegion (seg);
    SegmentFree (seg);
    return 0;
}




/*
 * Changes the read/write/execute protection attributes of a virtual
 * memory segment pointed to by 'addr'..
 */

SYSCALL int VirtualProtect (vm_addr addr, bits32_t flags)
{
    struct Segment *seg;
    struct Process *current;

    current = GetCurrentProcess();
    addr = ALIGN_DOWN (addr, PAGE_SIZE);
        
    if ((seg = SegmentFind (addr)) == NULL)
        return memoryErr;

//  if (seg->flags & VSEG_BUSY)
//      Sleep (&vm_busy_rendez);

    if (seg->owner != current || MEM_TYPE(seg->flags) == MEM_RESERVED)
        return memoryErr;

    DisablePreemption();

    PmapRemoveRegion (seg);
    seg->flags = (seg->flags & ~PROT_MASK) | (flags & PROT_MASK);
    PmapEnterRegion (seg, seg->base);
    return 0;
}




/*
 * Returns a 64-bit unique ID for a segment.  This ID is uniquely chosen
 * whenever a segment is allocated with VirtualAlloc() or VirtualAllocPhys().
 * The ID is updated to another unique value whenever a segment has the
 * MAP_segment_id flag set and a write is performed.  Once a write is performed
 * the MAP_segment_id flag has to be reenabled with a call to Virtualsegment_id().
 */

SYSCALL int VirtualVersion (vm_addr addr, uint64 *segment_id)
{
    struct Segment *seg;
    struct Process *current;

    current = GetCurrentProcess();
    addr = ALIGN_DOWN (addr, PAGE_SIZE);
    
    if ((seg = SegmentFind (addr)) == NULL || seg->owner != current
        || !(MEM_TYPE(seg->flags) == MEM_ALLOC || MEM_TYPE(seg->flags) == MEM_PHYS))
    {
        return memoryErr;
    }

//  if (seg->flags & VSEG_BUSY)
//      Sleep (&vm_busy_rendez);
    
    CopyOut (segment_id, &seg->segment_id, sizeof (uint64));

    DisablePreemption();
    
    seg->flags |= MAP_VERSION;
    return 0;
}




/*
 * Returns the size of a segment pointed to by addr
 */

SYSCALL ssize_t VirtualSizeOf (vm_addr addr)
{
    struct Segment *seg;
    struct Process *current;
    
    current = GetCurrentProcess();
    
    if ((seg = SegmentFind (addr)) == NULL || seg->owner != current)
        return memoryErr;

//  if (seg->flags & VSEG_BUSY)
//      Sleep (&vm_busy_rendez);
        
    return seg->size;
}




/*
 * Marks a segment of memory pointed to by 'addr' as immovable to prevent this
 * segment from being compacted. Returns the physical address of the segment
 * for DMA purposes.
 *
 * Sleeps if the compactor marks segment as busy, which indicates it either
 * intends to compact the memory once the wired flag is cleared or is currently
 * performing segment compaction.
 */

SYSCALL int WireMem (vm_addr addr, vm_addr *ret_phys)
{
    struct Process *current;
    struct Segment *seg;
    vm_addr phys;

    current = GetCurrentProcess();

    if (!(current->flags & PROCF_ALLOW_IO))
        return privilegeErr;
        
    if ((seg = SegmentFind (addr)) == NULL || seg->owner != current
        || MEM_TYPE(seg->flags) != MEM_ALLOC)
    {
        return memoryErr;
    }

    if (seg->flags & SEG_WIRED)
        return memoryErr;

    if (seg->flags & SEG_COMPACT)
        Sleep (&compact_rendez);
        
    phys = seg->physical_addr + (addr - seg->base);
    CopyOut (ret_phys, &phys, sizeof (vm_addr));

    DisablePreemption();
        
    seg->flags |= SEG_WIRED;
    
    PmapCachePreDMA (seg);
    
    return 0;
}




/*
 * Unwires a segment of memory previously flagged by WireMem()
 */

SYSCALL int UnwireMem (vm_addr addr)
{
    struct Process *current;
    struct Segment *seg;
    
    current = GetCurrentProcess();
    
    if (!(current->flags & PROCF_ALLOW_IO))
        return privilegeErr;
        
    addr = ALIGN_DOWN (addr, PAGE_SIZE);
        
    if ((seg = SegmentFind (addr)) == NULL || seg->owner != current
        || MEM_TYPE(seg->flags) != MEM_ALLOC)
    {
        return memoryErr;
    }
    
    if ((seg->flags & SEG_WIRED) == 0)
        return memoryErr;
    
    DisablePreemption();

    seg->flags &= ~SEG_WIRED;

    PmapCachePostDMA (seg);
    
    WakeupAll (&compact_rendez);
    return 0;
}





/*
 * Searches the cache for a segment belonging to the current process
 * and matching the segment_id parameter.
 */

SYSCALL vm_addr GetCachedSegment (uint64 *in_segment_id)
{
    struct Process *current;
    struct Segment *seg;
    uint64 segment_id;
    uint64 key;
    
    current = GetCurrentProcess();
    
    CopyIn (&segment_id, in_segment_id, sizeof (segment_id));
    
    key = segment_id % CACHE_HASH_SZ;
    
    seg = LIST_HEAD (&cache_hash[key]);

    DisablePreemption();
                
    while (seg != NULL)
    {
        if (seg->segment_id == segment_id && seg->owner == current)
        {
            LIST_REM_ENTRY (&cache_lru_list, seg, lru_link);
            LIST_REM_ENTRY (&cache_hash[key], seg, hash_link);

            if (seg == last_aged_seg)
                last_aged_seg = NULL;
                
            return seg->base;
        }
        
        seg = LIST_NEXT (seg, hash_link);
    }        

    return (vm_addr)NULL;
}




/*
 * Stores a segment in the cache, marks the current owner
 */

SYSCALL int PutCachedSegment (vm_addr addr, bits32_t flags, uint64 *out_segment_id)
{
    struct Process *current;
    struct Segment *seg;
    uint64 segment_id;
    uint64 key;
    
    current = GetCurrentProcess();
    
   
    seg = SegmentFind (addr);
    
    if (seg == NULL || MEM_TYPE(seg->flags) != MEM_ALLOC || seg->owner != current)
        return paramErr;

    segment_id = seg->segment_id;
    key = segment_id % CACHE_HASH_SZ;
    CopyOut (out_segment_id, &segment_id, sizeof (segment_id));

    DisablePreemption();

    LIST_ADD_HEAD (&cache_hash[key], seg, hash_link);

    if (flags & CACHE_AGE)
    {
        if (last_aged_seg != NULL) {
            LIST_INSERT_BEFORE (&cache_lru_list, last_aged_seg,  seg, lru_link);
        }
        else { 
            LIST_ADD_TAIL (&cache_lru_list, seg, lru_link);
        }
        
        last_aged_seg = seg;
    }
    else {
        LIST_ADD_HEAD (&cache_lru_list, seg, lru_link);    
    }
    
    PmapRemoveRegion (seg);

    return 0;
}




/* 
 *
 */

SYSCALL int ExpungeCachedSegment (uint64 *in_segment_id)
{
    struct Process *current;
    struct Segment *seg;
    uint64 segment_id;
    uint64 key;
    
    current = GetCurrentProcess();
    
    CopyIn (&segment_id, in_segment_id, sizeof (segment_id));
    
    key = segment_id % CACHE_HASH_SZ;
    
    seg = LIST_HEAD (&cache_hash[key]);

    DisablePreemption();
                
    while (seg != NULL)
    {
        if (seg->segment_id == segment_id && seg->owner == current)
        {
            LIST_REM_ENTRY (&cache_lru_list, seg, lru_link);
            LIST_REM_ENTRY (&cache_hash[key], seg, hash_link);

            if (seg == last_aged_seg)
                last_aged_seg = NULL;
            
            HEAP_ADD_HEAD (&free_segment_list[seg->bucket_q], seg, link);
            seg->size = 0;
            seg->physical_addr = (vm_addr)NULL;
            seg->flags = MEM_FREE;
            seg->owner = NULL;
            
            PmapRemoveRegion (seg);
            return 0;
        }
        else if (seg->segment_id == segment_id && seg->owner != current)
        {
            return memoryErr;
        }
        
        seg = LIST_NEXT (seg, hash_link);
    }
    
    return memoryErr;        
}















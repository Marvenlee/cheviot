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




/*
 * Enqueues a memory allocation request to the VM kernel task.
 */

struct Segment *SegmentAlloc (ssize_t size, bits32_t flags)
{
    struct Segment *seg;
    struct Process *current;
    
    current = GetCurrentProcess();

    if (current->virtualalloc_state == VIRTUALALLOC_STATE_READY)
    {
        if (size <= 0 || requested_alloc_size + size < requested_alloc_size)
            return NULL;
 
        current->virtualalloc_state = VIRTUALALLOC_STATE_SEND;       
        requested_alloc_size += size;
        current->virtualalloc_size = size;
        current->virtualalloc_flags = flags;
       
        Wakeup (&vm_task_rendez);    
        Sleep (&alloc_rendez);
        return NULL;    
    }
    else if (current->virtualalloc_state == VIRTUALALLOC_STATE_REPLY)
    {
        seg = current->virtualalloc_segment;
        current->virtualalloc_state = VIRTUALALLOC_STATE_READY;
        return seg;
    }
    else
    {
        current->virtualalloc_state = VIRTUALALLOC_STATE_READY;
        return NULL;
    }
}




/*
 * Allocates a segment struct on behalf of VirtualAllocPhys().
 */

struct Segment *SegmentAllocPhys (ssize_t size, bits32_t flags)
{
    int t;
    struct Segment *seg;
    struct Process *current;
    
    current = GetCurrentProcess();
    
    if (size <= 0)
        return NULL;
    
    for (t=0; t < NSEGBUCKET; t++)
    {
        if (size <= free_segment_size[t]
            && (seg = LIST_HEAD (&free_segment_list[t])) != NULL)
        {
            LIST_REM_HEAD (&free_segment_list[t], link);

            LIST_ADD_TAIL (&segment_heap, seg, link);
            
            seg->size = size;
            seg->physical_addr = (vm_addr)NULL;
            seg->flags = flags;
            seg->owner = current;

            return seg;
        }
    }
    
    return NULL;
}








/*
 * SegmentFree();
 *
 * Called by VirtualFree()
 * 
 * Frees a segment if it was created by VirtualAllocPhys().
 * Otherwise it marks the segment as garbage to be freed by
 * the 'compaction' task.
 */
 
void SegmentFree (struct Segment *seg)
{
    KASSERT (0 <= seg->bucket_q && seg->bucket_q < NSEGBUCKET);

    if (MEM_TYPE (seg->flags) == MEM_PHYS)
    {
        seg->flags = MEM_FREE;
        LIST_ADD_HEAD (&free_segment_list[seg->bucket_q], seg, link);
    }
    else
    {
        seg->flags = MEM_GARBAGE;
        garbage_size += seg->size;
    }
}
     





/* VMTask Part 1
 * 
 * The VM Task is a kernel task that processes requests to allocate and free memory
 * from the heap.  It resizes the segment cache and compacts the heap to make room
 * at the top for allocations.
 *
 * As the kernel has a single kernel stack (interrupt model kernel) the VM Task
 * is a collection of continuation functions that are called by KernelExit.  This way
 * the VM Task never returns to user-mode.  The VM task maps all of physical memory
 * into its address space and the page fault handling code treats it as a special case.
 * 
 * The VM Task is made up of 4 continuations that have a number of preemption points.
 * If the continuation is preempted it starts again the next time the VM Task runs.
 * If a continuation completes it disables preemption and advance to the next
 * continuation.
 *
 * 1 VMTaskBegin()
 * 2 VMTaskResizeCache()
 * 3 VMTaskCompactHeap()
 * 4 VMTaskAllocateSegments();
 *
 * VMTaskBegin determines if there is any requests to allocate memory, otherwise
 * it goes to sleep.
 */

void VMTaskBegin (void)
{
    struct Process *current;
    
    current = GetCurrentProcess();
    
    if (requested_alloc_size == 0)
        Sleep (&vm_task_rendez);
    
    DisablePreemption();
    current->continuation_function = &VMTaskResizeCache;
}





/* 
 * VM Task's 2nd continuation.
 * 
 * Resizes the segment cache by marking the least recently used segments as
 * garbage until the cache is of the desired size or less.
 */  
    
void VMTaskResizeCache (void)
{
    struct Segment *cache_seg;
    struct Segment *seg;
    vm_size heap_remaining;
    vm_addr top_of_heap;
    struct Process *current;
    vm_size desired_cache_size;
    
    
    current = GetCurrentProcess();

    seg = LIST_TAIL (&segment_heap);
        
    if (seg == NULL)
        top_of_heap = VM_USER_BASE;
    else
        top_of_heap = seg->base + seg->size;
    
    heap_remaining = memory_ceiling - top_of_heap;
    desired_cache_size = (heap_remaining + garbage_size + cache_size
                                               - requested_alloc_size) / 4 * 3;
        
    if (desired_cache_size < min_cache_size)
        desired_cache_size = min_cache_size;
    
    if (desired_cache_size > max_cache_size)
        desired_cache_size = max_cache_size;
    
    if (desired_cache_size > cache_size)
        desired_cache_size = cache_size;
    
    while (cache_size > desired_cache_size)
    {
        DisablePreemption();
    
        cache_seg = LIST_TAIL (&cache_lru_list);
        LIST_REM_TAIL (&cache_lru_list, lru_link);
        
        if (cache_seg == last_aged_seg)
            last_aged_seg = NULL;
        
        cache_seg->flags = MEM_GARBAGE;
        cache_size -= cache_seg->size;
        garbage_size += cache_seg->size;
        
        EnablePreemption();
    }
    
    
    DisablePreemption();
    current->continuation_function = &VMTaskCompactHeap;
}




/*
 * VM Task's 3rd continuation.
 * 
 * Frees segments marked as garbage and compacts allocated segments towards
 * the bottom of physical memory.  Memory compaction is preemptible with 
 * the current segment and offset of the move updated for every page moved.
 */
 

void VMTaskCompactHeap(void)
{
	struct Segment *prev;
    struct Segment *next;
    struct Process *current;
    vm_addr watermark;    
    

    current = GetCurrentProcess();
   
    DisablePreemption();
       
    if (garbage_size == 0)
    {
        current->continuation_function = &VMTaskAllocateSegments;
        return;
    }
    
    if (compact_seg == NULL)
    {
        compact_seg = LIST_HEAD (&segment_heap);
        compact_offset = 0;
        watermark = VM_USER_BASE;
    }
    

	while (compact_seg != NULL)
	{
        if (MEM_TYPE(compact_seg->flags) == MEM_GARBAGE)
        {
            next = HEAP_NEXT (compact_seg, link);
            
            EnablePreemption();
            DisablePreemption();
        
            HEAP_REM_ENTRY (&segment_heap, compact_seg, link);
            HEAP_ADD_HEAD (&free_segment_list[compact_seg->bucket_q], compact_seg, link);
            compact_seg->size = 0;
            compact_seg->physical_addr = (vm_addr)NULL;
            compact_seg->flags = MEM_FREE;
            compact_seg->owner = NULL;
            compact_seg = next;
        }
        else
        {        
            if (compact_offset == 0)
            {
                prev = LIST_PREV (compact_seg, link);
                
                if (prev != NULL)
                    watermark = prev->base + prev->size;
                else
                    watermark = VM_USER_BASE;
                
                if (compact_seg->size >= 0x100000)
                    watermark = ALIGN_UP (watermark, 0x100000);
                else if (compact_seg->size >= 0x10000)
                    watermark = ALIGN_UP (watermark, 0x10000);
            }            
              
            if (watermark < compact_seg->physical_addr)
            {
                if (compact_seg->flags & SEG_WIRED)
                {
                    compact_seg->flags |= SEG_COMPACT;
                    Sleep (&compact_rendez);
                }
                
                if (compact_seg->owner != NULL)
                    PmapRemoveRegion (compact_seg->owner->pmap, compact_seg);
                                                
                EnablePreemption();
            
                while (compact_offset < compact_seg->size)
                {
                    MemCpy ((void *)(watermark + compact_offset),
                        (void *)(compact_seg->physical_addr + compact_offset),
                        PAGE_SIZE);
                        
                    DisablePreemption();
                    compact_offset += PAGE_SIZE;
                    EnablePreemption();
                }
                
                DisablePreemption();
            
                compact_seg->physical_addr = watermark;
                compact_offset = 0;
                
                compact_seg->flags &= ~SEG_COMPACT;
                WakeupAll (&compact_rendez);
            }
    
            DisablePreemption();
            compact_seg = LIST_NEXT (compact_seg, link);
        }
    }
    
    DisablePreemption();

    compact_seg = NULL;
    compact_offset = 0;

    current->continuation_function = &VMTaskAllocateSegments;
}




/*
 * VM Task's 4th continuation.
 *
 * Processes the requested allocations and stores information about the allocated
 * segment in the client's Process struct.  If there is new free segments and not
 * enough space at the top of the heap the VM Task restarts from the 1st continuation,
 * compacts and tries again.
 */

void VMTaskAllocateSegments (void)
{
    struct Process *proc;
    struct Segment *seg;
    struct Process *current;

    
    current = GetCurrentProcess();
    
    while((proc = LIST_HEAD (&alloc_rendez.process_list)) != NULL)
    {
        seg = AllocateSegment (proc, proc->virtualalloc_size, proc->virtualalloc_flags);

        if (seg == NULL && garbage_size > 0 && cache_size > min_cache_size)
            break;
                
        requested_alloc_size -= proc->virtualalloc_size;
        proc->virtualalloc_segment = seg;
        proc->virtualalloc_state = VIRTUALALLOC_STATE_REPLY;

        Wakeup (&alloc_rendez);
    }

    DisablePreemption();
    current->continuation_function = &VMTaskBegin;
}




/*
 * Allocates a single segment at the top of the heap.
 */
 
struct Segment *AllocateSegment (struct Process *proc, vm_size size, bits32_t flags)
{
    vm_addr addr;
    vm_addr top_of_heap;
    int t;
    struct Segment *seg;
  
    seg = LIST_TAIL (&segment_heap);
    
    top_of_heap = seg->physical_addr + seg->size;
    
    addr = top_of_heap;

    if (seg->size > 0x100000)
        addr = ALIGN_UP (top_of_heap, 0x100000);
    else if (seg->size > 0x10000)
        addr = ALIGN_UP (top_of_heap, 0x10000);
    else
        addr = top_of_heap;

    if (addr + size > addr && addr + size  <= memory_ceiling)
    {
        for (t=0; t < NSEGBUCKET; t++)
        {
            if (size <= free_segment_size[t]
                && (seg = LIST_HEAD (&free_segment_list[t])) != NULL)
            {
                LIST_REM_HEAD (&free_segment_list[t], link);

                LIST_ADD_TAIL (&segment_heap, seg, link);
                
                seg->size = size;
                seg->physical_addr = addr;
                seg->flags = flags;
                seg->owner = proc;
 
                return seg;
            }
        }

    }

    return NULL;
}




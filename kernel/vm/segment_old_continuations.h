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

void VMTaskInitializeCompaction (void);
void VMTaskCompactSegments (void);
void VMTaskAllocateSegments (void);
void VMTaskWakeupClients (void);
struct Segment *SegmentAllocStruct (struct Process *proc);
void VMTaskClearSegment (void);
void VMTaskReply (void);

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
 * Sends an allocation request to the VM kernel task.
 */

struct Segment *SegmentAlloc (vm_addr paddr, ssize_t size, bits32_t flags)
{
    struct Segment *seg;
    struct Process *current;
    
    current = GetCurrentProcess();
    
    if (size <= 0)
        return NULL;
    
    
    DisablePreemption();
        
    if (current->virtualalloc_state == VIRTUALALLOC_STATE_READY)
    {
        current->virtualalloc_state = VIRTUALALLOC_STATE_SEND;
        current->virtualalloc_paddr = paddr;       
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
        KernelPanic ("Invalid virtualalloc state");
        return NULL;
    }
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
     





/*
 *
 */

void VMTaskBegin (void)
{
    if (requested_alloc_size == 0)
    {
        Sleep (&vm_task_rendez);
    }
    
    ContinueTo (&VMTaskResizeCache);
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
    vm_size desired_cache_size;
    
    
    DisablePreemption();

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
        cache_seg = LIST_TAIL (&cache_lru_list);
        LIST_REM_TAIL (&cache_lru_list, lru_link);
        
        if (cache_seg == last_aged_seg)
            last_aged_seg = NULL;
        
        cache_seg->flags = MEM_GARBAGE;
        cache_size -= cache_seg->size;
        garbage_size += cache_seg->size;
        
        EnablePreemption();
        DisablePreemption();
    }
    

    ContinueTo (&VMTaskCompactHeap);
}




/*
 * VM Task's 3rd continuation.
 * 
 * Frees segments marked as garbage and compacts allocated segments towards
 * the bottom of physical memory.  Memory compaction is preemptible with 
 * the current segment and offset of the move updated for every page moved.
 */
 

void VMTaskCompactHeap (void)
{
	struct Segment *prev;
    struct Segment *next;
    vm_addr watermark;    
    

    DisablePreemption();
        
    if (garbage_size == 0)
        ContinueTo (&VMTaskWakeupClients);

    
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
                
                PmapRemoveRegion (compact_seg);
                                                
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
    
    ContinueTo (&VMTaskWakeupClients);
}




/*
 * VM Task's 4th continuation.
 *
 * Wakes up threads that are blocked waiting for the compaction to finish.
 */

void VMTaskWakeupClients (void)
{
    struct Process *proc;
    struct Segment *seg;


    DisablePreemption();
    
    while (LIST_HEAD (&alloc_rendez.process_list)) == NULL)
    {
        Wakeup (&alloc_rendez);
    }
    
    ContinueTo (&VMTaskBegin);
}
        

































        
/*
 *
 */ 
 
void VMTaskAllocateSegment (void)
{
    struct Segment *segment;
    struct Process *proc;
    
        
        
    alloc_seg = SegmentAllocStruct (proc);
    
    if (alloc_seg == NULL && (garbage_size > 0 || cache_size > min_cache_size))
        ContinueTo (&VMTaskResizeCache);
    
    if (alloc_seg == NULL)
    {
        requested_alloc_size -= proc->virtualalloc_size;
        proc->virtualalloc_size = 0;
        proc->virtualalloc_segment = NULL;
        proc->virtualalloc_state = VIRTUALALLOC_STATE_REPLY;
        Wakeup (&alloc_rendez);
        ContinueTo (&VMTaskWakeupClients);
    }
    
    ContinueTo (&VMTaskClearSegment);
    
    
}








/*
 * 
 */

struct Segment *SegmentAllocStruct (struct Process *proc)
{
    int t;
    struct Segment *seg;
    
    // Round up size

    
    for (t=0; t < NSEGBUCKET; t++)
    {
        if (proc->virtualalloc_size <= free_segment_size[t]
            && (seg = LIST_HEAD (&free_segment_list[t])) != NULL)
        {
            LIST_REM_HEAD (&free_segment_list[t], link);

            LIST_ADD_TAIL (&segment_heap, seg, link);
            
            seg->size = proc->virtualalloc_size;
            
//            seg->physical_addr = proc->virtualalloc_paddr;
            
            seg->flags = proc->virtualalloc_flags;
            seg->owner = proc;
            
            return seg;
        }
    }
    
    return NULL;
}












vm_addr clean_seg_base;
vm_size clean_seg_size;
vm_offset clean_seg_offset;



/*
 *
 */

void VMTaskClearSegment (void)
{
    current->seg_clear_offset;

    while (clean_seg_offset < clean_seg_size)
    {
        MemSet ((void *)(clean_seg_base + clean_seg_offset), PAGE_SIZE, 0);
    
        DisablePreemption();
        clean_seg_offset += PAGE_SIZE;
        EnablePreemption();
    }
    

        
    ContinueTo (&VMTaskReply);
}


























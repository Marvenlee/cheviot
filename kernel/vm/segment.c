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



SYSCALL int SysVMRecvMsg (struct VMMsg *msg);
SYSCALL int SysVMReplyMsg (struct VMMsg *msg);
SYSCALL size_t SysVMResizeCache (size_t desired_cache_size);

SYSCALL int SysVMLockSegment (struct Segment *segment);
SYSCALL int SysVMUnlockSegment (struct Segment *segment);

SYSCALL struct Segment *SysVMAllocSegment (struct Process *proc, size_t size, bits32_t flags, vm_addr paddr);
SYSCALL int SysVMFreeSegment (struct Segment *seg);
SYSCALL int SysVMFreeProcessSegments (struct Process *proc);




SYSCALL int VMRecvMsg (struct VMMsg *msg);
SYSCALL int VMReplyMsg (struct VMMsg *msg);
SYSCALL size_t VMResizeCache (size_t desired_cache_size);

SYSCALL int VMLockSegment (struct Segment *segment);
SYSCALL int VMUnlockSegment (struct Segment *segment);

SYSCALL struct Segment *VMAllocSegment (struct Process *proc, size_t size, bits32_t flags, vm_addr paddr);
SYSCALL int VMFreeSegment (struct Segment *seg);
SYSCALL int VMFreeProcessSegments (struct Process *proc);

void CompactMem (size_t requested_alloc_size);








/*
 * Sends an allocation request to the VM kernel task.
 */

struct Segment *SegmentAlloc (vm_addr paddr, vm_size size, bits32_t flags)
{
    struct Segment *seg;
    struct Process *current;
    
    current = GetCurrentProcess();
    
    if (size <= 0)
        return NULL;
    
    if (current->vm_msg_state == VM_STATE_READY)
    {
        current->vm_msg.type = VMMSG_ALLOC;
        current->vm_msg.paddr = paddr;       
        current->vm_msg.size = size;
        current->vm_msg.flags = flags;
        
        current->vm_msg_state = VM_STATE_SEND;       
        Wakeup (&vm_task_rendez);    
        Sleep (&alloc_rendez);
        return NULL;    
    }
    else if (current->vm_msg_state == VM_STATE_REPLY)
    {
        seg = current->vm_msg.seg;
        current->vm_msg_state = VM_STATE_READY;
        return seg;
    }
    else
    {
        return NULL;
    }
}




/*
 * SegmentFree();
 *
 * Called by VirtualFree()
 * 
 */
 
void SegmentFree (struct Segment *seg)
{
    struct Process *current;
    
    current = GetCurrentProcess();
    
    if (size <= 0)
        return NULL;
    
    if (current->vm_msg_state == VM_STATE_READY)
    {
        current->vm_msg.type = VMMSG_FREE;
        current->vm_msg.seg = seg;
        
        current->vm_msg_state = VM_STATE_SEND;       
        Wakeup (&vm_task_rendez);    
        Sleep (&alloc_rendez);
        return NULL;    
    }
    else if (current->vm_msg_state == VM_STATE_REPLY)
    {
        current->vm_msg_state = VM_STATE_READY;
        return seg;
    }
    else
    {
        return NULL;
    }
}






/*
 *
 */

void VMTask (void)
{
    struct VMMsg msg;
    struct Segment *seg;
    
    
    while (1)
    {
        SysVMRecvMsg (&msg);
     
        if (msg.type == VMMSG_ALLOC)
        {                   
            if ((seg = SysVMAllocSegment (msg.proc, msg.size, msg.flags, msg.paddr)) == NULL)
            {
                CompactMem(msg.size);
                seg = SysVMAllocSegment (msg.proc, msg.size, msg.flags, msg.paddr);
            }
            
            
            msg.seg = seg;
        }
        else if (msg.type == VMMSG_FREE)
        {
            msg.result = SysVMFreeSegment(msg.seg);
            msg.seg = NULL;
        }
        else if (msg.type == VMMSG_FREE_PROCESS)
        {
            SysVMFreeProcessSegments (msg.proc);
            msg.seg = NULL;
        }
        
        SysVMReplyMsg (&msg);
    }
        
}







        
        
        
        
                
void CompactMem (size_t requested_alloc_size)
{                
    struct Segment *seg;
    vm_addr watermark;
    vm_size desired_cache_size;
    vm_size heap_remaining;      
    vm_addr top_of_heap;
    
    
    seg = LIST_TAIL (&segment_heap);
        
    if (seg == NULL)
        top_of_heap = VM_USER_BASE;
    else
        top_of_heap = seg->base + seg->size;
    
    heap_remaining = memory_ceiling - top_of_heap;


    
    desired_cache_size = (heap_remaining + garbage_size + cache_size
                                           - requested_alloc_size) / 4 * 3;

    VMResizeCache (desired_cache_size);


    watermark = VM_USER_BASE;
    
    seg = LIST_HEAD (&segment_heap);
    
    while (seg != NULL)
    {
        alignment = PmapAlignmentSize (seg->size);
    
        if (seg->size >= 0x100000)
            watermark = ALIGN_UP (watermark, 0x100000);
        else if (compact_seg->size >= 0x10000)
            watermark = ALIGN_UP (watermark, 0x10000);

        if (watermark >= seg->physical_addr)
            watermark = new_watermark;
            
        VMLockSegment (seg);

        if (watermark < seg->physical_addr)
        {
            MemCpy ((void *)watermark, (void *) seg->physical_addr, seg->size);
            seg->physical_addr = watermark;
        }
        
        VMUnlockSegment (seg);
        
        watermark = seg->physical_addr + seg->size;
        
        seg = LIST_NEXT (seg, link);
    }
}








SYSCALL int VMRecvMsg (struct VMMsg *msg)
{
    struct Process *current;
    
    
    current = GetCurrentProcess();

    if ((current->flags & PROCF_DAEMON) == 0)
        return -1;
    
    
    // if alloc queue has entry
    // copy out data, disablepreemption, removed head
    
    // else sleep
    
    
    
    return 0;
}




SYSCALL int VMReplyMsg (struct VMMsg *msg)
{
    struct Process *current;
    
    
    current = GetCurrentProcess();

    if ((current->flags & PROCF_DAEMON) == 0)
        return -1;
    
    // reply current message to proc in msg???????
    // wakeup blocked task.
        
    return 0;
}




/*
 *
 */  
    
SYSCALL size_t VMResizeCache (size_t desired_cache_size)
{
    struct Segment *cache_seg;
    struct Process *current;
    
    
    current = GetCurrentProcess();

    if ((current->flags & PROCF_DAEMON) == 0)
        return -1;
        
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
        
        cached_seg->flags = MEM_FREE;
        LIST_REM_ENTRY (&segment_heap, cached_seg, link);
        LIST_ADD_HEAD (&free_segment_list[seg->bucket_q], cached_seg, link);
        
        cache_size -= cache_seg->size;
        garbage_size += cache_seg->size;
        
        EnablePreemption();
    }
    
    return cache_size;
}




/*
 */
 
 
int VMLockSegment (struct Segment *seg)
{
    struct Process *current;
    
    
    current = GetCurrentProcess();

    if ((current->flags & PROCF_DAEMON) == 0)
        return -1;
    
    DisablePreemption();
    
    PmapRemoveRegion (seg);
    
    if (seg->flags & vm_busy);
        Sleep (&blah_rendez);
    
    seg->flags |= blah;
    
    return 0;
}




/*
 *
 */

int VMUnlockSegment (struct Segment *seg)
{
    struct Process *current;
    
    
    current = GetCurrentProcess();

    if ((current->flags & PROCF_DAEMON) == 0)
        return -1;
    
    seg->flags &= ~blah;
    WakeupAll (&blah_rendez);    
    
    return 0;
}
 
 
 
 
 



        
/*
 *
 */ 
 
struct Segment *VMAllocSegment (struct Process *proc, size_t size, bits32_t flags, vm_addr paddr)
{
    struct Segment *seg;
    struct Process *current;
    vm_size heap_remaining;      
    vm_addr top_of_heap;
    vm_size alignment;
    
    current = GetCurrentProcess();
    
    if ((current->flags & PROCF_DAEMON) == 0)
        return NULL;
    
    if (MEM_TYPE(flags) == MEM_ALLOC)
    {
        seg = LIST_TAIL (&segment_heap);
            
        if (seg == NULL)
            top_of_heap = VM_USER_BASE;
        else
            top_of_heap = seg->base + seg->size;
        
        heap_remaining = memory_ceiling - top_of_heap;

        alignment = PmapAlignmentSize (size);
        
        paddr = ALIGN_UP (top_of_heap, alignment);
        
        if (paddr >= top_of_heap && paddr + size > top_of_heap && paddr + size <= memory_ceiling)
        {
            if ((seg = SegmentAllocStruct (size)) != NULL)
            {
                seg->owner = NULL;
                seg->size = size;
                seg->flags = flags;
                seg->physical_addr = paddr;
            }
        }
        else
        {
            seg = NULL;
        }

    }
    else if (MEM_TYPE(flags) == MEM_PHYS)
    {
        if ((seg = SegmentAllocStruct (size)) != NULL)
        {
            seg->owner = NULL;
            seg->size = size;
            seg->flags = flags;
            seg->physical_addr = paddr;
        }
    }
    else
    {
        seg = NULL;
    }
    
    return seg;
    
}




/*
 * 
 */

struct Segment *SegmentAllocStruct (size_t size)
{
    int t;
    struct Segment *seg;
    
    
    for (t=0; t < NSEGBUCKET; t++)
    {
        if (size <= free_segment_size[t]
            && (seg = LIST_HEAD (&free_segment_list[t])) != NULL)
        {
            LIST_REM_HEAD (&free_segment_list[t], link);
            LIST_ADD_TAIL (&segment_heap, seg, link);
            seg->size = size;
            return seg;
        }
    }
    
    return NULL;
}













/*
 *
 */

int VMFreeSegment (struct Segment *seg)
{
    struct Process *current;


    KASSERT (0 <= seg->bucket_q && seg->bucket_q < NSEGBUCKET);
    
    current = GetCurrentProcess();

    if ((current->flags & PROCF_DAEMON) == 0)
        return -1;

    if (MEM_TYPE (seg->flags) == MEM_PHYS)
    {
        seg->flags = MEM_FREE;
        LIST_ADD_HEAD (&free_segment_list[seg->bucket_q], seg, link);
    }
    else
    {
        seg->flags = MEM_FREE;
        LIST_REM_ENTRY (&segment_heap, seg, link);
        LIST_ADD_HEAD (&free_segment_list[seg->bucket_q], seg, link);
    }

    return 0;
}





/*
 *
 */

int VMFreeProcessSegments (struct Process *proc)
{
    struct Process *current;
    
    
    current = GetCurrentProcess();

    if ((current->flags & PROCF_DAEMON) == 0)
        return -1;
        
    return 0;
}











































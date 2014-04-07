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
 * The timeout watchdog is currently unimplemented.  A way of notifying
 * the Executive to free cached file segments is currently unimplemented.
 *
 * The look up and creation of the virtseg and freeseg data structures
 * has preemption disabled.  Preemption is disabled for the duration of
 * cleaning the allocated memory.
 *
 * This could be optimized to allow preemption during the lookup of
 * suitable sized segments, but any resizing or changes to the
 * virtseg_table and [hysseg_table requiring preemption to be disabled.
 *
 * The cleaning of the allocated memory could be done with preemption
 * enabled in a continuation.  Progress could be saved every few kilobytes
 * within the process structure.
 */

SYSCALL vm_addr VirtualAlloc (vm_addr addr, vm_size len, bits32_t flags)
{
	struct PhysicalSegment *ps;
	struct VirtualSegment *vs;
	struct Process *current;
    vm_addr va;

		
	current = GetCurrentProcess();
	
	if (CheckWatchdog() == -1)
	    return (vm_addr)NULL;
	
	if (virtseg_cnt >= (max_virtseg - 3) || physseg_cnt >= (max_physseg - 3))
        Sleep (&vm_rendez);

	addr = ALIGN_DOWN (addr, PAGE_SIZE);
	len = ALIGN_UP (len, PAGE_SIZE);
	
	flags = (flags & ~VM_SYSTEM_MASK) | MEM_ALLOC;

	if (PmapSupportsCachePolicy (flags) == -1)
	{
	    SetWatchdog (0);
	    return (vm_addr)NULL;
	}
	
	DisablePreemption();
		
	if ((vs = VirtualSegmentCreate (addr, len, flags)) == NULL)
	    Sleep(&vm_rendez);
	
	if ((ps = PhysicalSegmentCreate (0, vs->ceiling - vs->base, flags)) == NULL)
	{
		VirtualSegmentDelete (vs);
	    Sleep(&vm_rendez);
	}
	
	ps->virtual_addr = vs->base;
	vs->owner = current;
	vs->physical_addr = ps->base;
	vs->version = segment_version_counter;
	segment_version_counter ++;
	
	vs->flags = (flags & ~PROT_MASK) | PROT_READWRITE;
	PmapEnterRegion (&current->pmap, vs, vs->base);
	
	for (va = vs->ceiling - PAGE_SIZE; va >= vs->base; va -= PAGE_SIZE)
		MemSet ((void *)va, 0, PAGE_SIZE);
	
	vs->flags = flags;
	PmapRemoveRegion (&current->pmap, vs);
	PmapEnterRegion (&current->pmap, vs, vs->base);
	
	SetWatchdog (0);
	return vs->base;
	
}




/*
 * Maps an area of physical memory such as IO devie or framebuffer into the
 * address space of the calling process.
 * Should be renamed to VirtualMapPhys
 */

SYSCALL vm_addr VirtualAllocPhys (vm_addr addr, vm_size len, bits32_t flags, vm_addr paddr)
{
	struct VirtualSegment *vs;
	struct Process *current;


	current = GetCurrentProcess();

	if (CheckWatchdog() == -1)
	    return (vm_addr)NULL;

	if (!(current->flags & PROCF_ALLOW_IO))
	{
	    SetWatchdog (0);
		return (vm_addr)NULL;
	}
	
	if (virtseg_cnt >= (max_virtseg - 3))
        Sleep (&vm_rendez);

	addr = ALIGN_DOWN (addr, PAGE_SIZE);
	paddr = ALIGN_DOWN (paddr, PAGE_SIZE);
	len = ALIGN_UP (len, PAGE_SIZE);

	flags = (flags & ~VM_SYSTEM_MASK) | MEM_PHYS;

	if (PmapSupportsCachePolicy (flags) == -1)
	{
	    SetWatchdog (0);
	    return (vm_addr)NULL;
    }
    
	DisablePreemption();

	if ((vs = VirtualSegmentCreate (addr, len, flags)) == NULL)
	{
	    Sleep(&vm_rendez);
    }
    	
	vs->owner = current;
	vs->physical_addr = paddr;
	vs->flags = flags;
	vs->version = segment_version_counter;
    segment_version_counter ++;

	PmapEnterRegion (&current->pmap, vs, vs->base);
	
	SetWatchdog (0);
	return vs->base;
}




/*
 *
 */

SYSCALL int VirtualFree (vm_offset addr)
{
	struct VirtualSegment *vs;
	struct PhysicalSegment *ps;
	struct Process *current;
	

	current = GetCurrentProcess();

	addr = ALIGN_DOWN (addr, PAGE_SIZE);

	if ((vs = VirtualSegmentFind (addr)) == NULL || vs->owner != current)
		return memoryErr;
	
	DisablePreemption();
	
// FIXME: Check to see if segment is busy?  Sleep if needed??????
			
	if (MEM_TYPE (vs->flags) == MEM_ALLOC)
	{	
		PmapRemoveRegion (&current->pmap, vs);

		ps = PhysicalSegmentFind (vs->physical_addr);
		PhysicalSegmentDelete (ps);
		VirtualSegmentDelete (vs);
        Wakeup (&vm_rendez);
		return 0;
	}
	else if (MEM_TYPE (vs->flags) == MEM_PHYS)
	{
		PmapRemoveRegion (&current->pmap, vs);
		VirtualSegmentDelete (vs);
        Wakeup (&vm_rendez);
		return 0;
	}
	else
	{
		return memoryErr;
	}
}




/*
 * Changes the read/write/execute protection attributes of a virtual
 * memory segment pointed to by 'addr'..
 */

SYSCALL int VirtualProtect (vm_addr addr, bits32_t flags)
{
	struct VirtualSegment *vs;
	struct Process *current;

	
	current = GetCurrentProcess();
	addr = ALIGN_DOWN (addr, PAGE_SIZE);
		
	if ((vs = VirtualSegmentFind (addr)) == NULL)
		return memoryErr;
		
	if (vs->owner != current || MEM_TYPE(vs->flags) == MEM_RESERVED)
	    return memoryErr;

	DisablePreemption();

	PmapRemoveRegion (&current->pmap, vs);
    vs->flags = (vs->flags & ~PROT_MASK) | (flags & PROT_MASK);
	PmapEnterRegion (&current->pmap, vs, vs->base);
	return 0;
}




/*
 * Returns a 64-bit unique ID for a segment.  This ID is uniquely chosen
 * whenever a segment is allocated with VirtualAlloc() or VirtualAllocPhys().
 * The ID is updated to another unique value whenever a segment has the
 * MAP_VERSION flag set and a write is performed.  Once a write is performed
 * the MAP_VERSION flag has to be reenabled with a call to VirtualVersion().
 */

SYSCALL int VirtualVersion (vm_addr addr, uint64 *version)
{
	struct VirtualSegment *vs;
    struct Process *current;
    
	current = GetCurrentProcess();
	addr = ALIGN_DOWN (addr, PAGE_SIZE);
	
	if ((vs = VirtualSegmentFind (addr)) == NULL || vs->owner != current
	    || !(MEM_TYPE(vs->flags) == MEM_ALLOC || MEM_TYPE(vs->flags) == MEM_PHYS))
	{
	    return memoryErr;
	}
    
    CopyOut (version, &vs->version, sizeof (uint64));

    DisablePreemption();
    
    vs->flags |= MAP_VERSION;
    return 0;
}

 


/*
 * Returns the size of a segment pointed to by addr
 */

SYSCALL ssize_t VirtualSizeOf (vm_addr addr)
{
	struct VirtualSegment *vs;
	struct Process *current;
	

	current = GetCurrentProcess();
	
	if ((vs = VirtualSegmentFind (addr)) == NULL || vs->owner != current)
		return memoryErr;
		
	return vs->ceiling - vs->base;
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
	struct VirtualSegment *vs;
	vm_addr phys;


	current = GetCurrentProcess();

	if (!(current->flags & PROCF_ALLOW_IO))
	    return privilegeErr;
		
	if ((vs = VirtualSegmentFind (addr)) == NULL || vs->owner != current
        || MEM_TYPE(vs->flags) != MEM_ALLOC)
	{
		return memoryErr;
    }
	
	phys = vs->physical_addr + (addr - vs->base);
	CopyOut (ret_phys, &phys, sizeof (vm_addr));
	
	if (vs->wired == TRUE)
	    return memoryErr;
		
	DisablePreemption();
	
	if (vs->busy == TRUE)
		Sleep (&vm_rendez);
	
	vs->wired = TRUE;
	return 0;
}




/*
 * Unwires a segment of memory previously flagged by WireMem()
 */

SYSCALL int UnwireMem (vm_addr addr)
{
	struct Process *current;
	struct VirtualSegment *vs;


	current = GetCurrentProcess();

	if (!(current->flags & PROCF_ALLOW_IO))
	    return privilegeErr;
		
	addr = ALIGN_DOWN (addr, PAGE_SIZE);
		
	if ((vs = VirtualSegmentFind (addr)) == NULL || vs->owner != current
	    || MEM_TYPE(vs->flags) != MEM_ALLOC)
	{
		return memoryErr;
    }
    
	if (vs->wired == FALSE)
	    return memoryErr;

	DisablePreemption();
	
	vs->wired = FALSE;
	Wakeup (&vm_rendez);
	return 0;
}
















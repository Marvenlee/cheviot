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
 *
 */

SYSCALL vm_addr VirtualAlloc (vm_addr addr, vm_size len, bits32_t flags)
{
	struct Segment *seg;
	struct MemArea *ma;
	struct Process *current;
    vm_addr va;

		
	current = GetCurrentProcess();
	
	if (CheckWatchdog() == -1)
	    return (vm_addr)NULL;
	
	if (memarea_cnt >= (max_memarea - 3) || segment_cnt >= (max_segment - 3))
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
		
	if ((ma = MemAreaCreate (addr, len, flags)) == NULL)
	    Sleep(&vm_rendez);
	
	if ((seg = SegmentCreate (0, ma->ceiling - ma->base, flags)) == NULL)
	{
		MemAreaDelete (ma);
	    Sleep(&vm_rendez);
	}
	
	seg->memarea = ma;
	ma->owner_process = current;
	ma->physical_addr = seg->base;
	ma->version = memarea_version_counter;

	memarea_version_counter ++;
	
	ma->flags = (flags & ~PROT_MASK) | PROT_READWRITE;
	PmapEnterRegion (&current->pmap, ma, ma->base);
	
	for (va = ma->ceiling - PAGE_SIZE; va >= ma->base; va -= PAGE_SIZE)
		MemSet ((void *)va, 0, PAGE_SIZE);
	
	ma->flags = flags;
	PmapRemoveRegion (&current->pmap, ma);
	PmapEnterRegion (&current->pmap, ma, ma->base);
	
	SetWatchdog (0);
	return ma->base;
	
}




/*
 * 
 */

SYSCALL vm_addr VirtualAllocPhys (vm_addr addr, vm_size len, bits32_t flags, vm_addr paddr)
{
	struct MemArea *ma;
	struct Process *current;


	current = GetCurrentProcess();

	if (CheckWatchdog() == -1)
	    return (vm_addr)NULL;

	if (!(current->flags & PROCF_ALLOW_IO))
	{
	    SetWatchdog (0);
		return (vm_addr)NULL;
	}
	
	if (memarea_cnt >= (max_memarea - 3))
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

	if ((ma = MemAreaCreate (addr, len, flags)) == NULL)
	{
	    Sleep(&vm_rendez);
    }
    	
	ma->owner_process = current;
	ma->physical_addr = paddr;
	ma->flags = flags;
	ma->version = memarea_version_counter;
    memarea_version_counter ++;

	PmapEnterRegion (&current->pmap, ma, ma->base);
	
	SetWatchdog (0);
	return ma->base;
}




/*
 *
 */

SYSCALL int VirtualFree (vm_offset addr)
{
	struct MemArea *ma;
	struct Segment *seg;
	struct Process *current;
	

	current = GetCurrentProcess();

	addr = ALIGN_DOWN (addr, PAGE_SIZE);

	if ((ma = MemAreaFind (addr)) == NULL || ma->owner_process != current)
		return memoryErr;
	
	DisablePreemption();
			
	if (MEM_TYPE (ma->flags) == MEM_ALLOC)
	{	
		PmapRemoveRegion (&current->pmap, ma);
		
		if (ma->msg_handle != -1)
		{   
		    handle_table[ma->msg_handle].owner = current;
		    CloseHandle (ma->msg_handle);
		    ma->msg_handle = -1;
		}
		
		seg = SegmentFind (ma->physical_addr);
		SegmentDelete(seg);
		MemAreaDelete (ma);
        Wakeup (&vm_rendez);
		return 0;
	}
	else if (MEM_TYPE (ma->flags) == MEM_PHYS)
	{
		PmapRemoveRegion (&current->pmap, ma);
		MemAreaDelete (ma);
        Wakeup (&vm_rendez);
		return 0;
	}
	else
	{
		return memoryErr;
	}
}






/*
 *
 */

SYSCALL int VirtualProtect (vm_addr addr, bits32_t flags)
{
	struct MemArea *ma;
	struct Process *current;

	
	current = GetCurrentProcess();
	addr = ALIGN_DOWN (addr, PAGE_SIZE);
		
	if ((ma = MemAreaFind (addr)) == NULL)
		return memoryErr;
		
	if (ma->owner_process != current || MEM_TYPE(ma->flags) == MEM_RESERVED)
	    return memoryErr;

	DisablePreemption();

	PmapRemoveRegion (&current->pmap, ma);
		
	ma->flags = (ma->flags & ~PROT_MASK) | (flags & PROT_MASK);
	PmapEnterRegion (&current->pmap, ma, ma->base);
	return 0;
}




/*
 * Returns a 64-bit unique ID for a segment.  This ID is uniquely chosen
 * whenever a segment is allocated with VirtualAlloc() or VirtualAllocPhys().
 * The ID is updated to another unique value whenever a segment has the
 * MAP_VERSION flag set and write is performed.  Once a write is performed
 * the MAP_VERSION flag has to be reenabled with a call to VirtualVersion().
 */

SYSCALL int VirtualVersion (vm_addr addr, uint64 *version)
{
	struct MemArea *ma;
    struct Process *current;
    
	current = GetCurrentProcess();
	addr = ALIGN_DOWN (addr, PAGE_SIZE);
	
	if ((ma = MemAreaFind (addr)) == NULL || ma->owner_process != current
	    || MEM_TYPE(ma->flags) != MEM_ALLOC)
	{
	    return memoryErr;
	}
    
    CopyOut (version, &ma->version, sizeof (uint64));

    DisablePreemption();
    
    ma->flags |= MAP_VERSION;
    return 0;
}

 


/*
 * Returns the size of a segment pointed to by addr
 */

SYSCALL ssize_t VirtualSizeOf (vm_addr addr)
{
	struct MemArea *ma;
	struct Process *current;
	

	current = GetCurrentProcess();
	
	if ((ma = MemAreaFind (addr)) == NULL || ma->owner_process != current)
		return memoryErr;
		
	return ma->ceiling - ma->base;
}




/*
 * Flags a segment of memory pointed to by 'addr' as immovable to prevent the
 * physical memory compaction system call CompactMem() from moving it. This
 * returns the physical address of the segment and is to be used prior to
 * performing DMA access.
 */

SYSCALL int WireMem (vm_addr addr, vm_addr *ret_phys)
{
	struct MemArea *ma;
	struct Process *current;
	struct Segment *seg;
	vm_addr phys;


	current = GetCurrentProcess();

	if (!(current->flags & PROCF_ALLOW_IO))
	    return privilegeErr;
		
	if ((ma = MemAreaFind (addr)) == NULL || ma->owner_process != current
        || MEM_TYPE(ma->flags) != MEM_ALLOC)
	{
		return memoryErr;
    }
	
	seg = SegmentFind (ma->physical_addr);
	phys = ma->physical_addr + (addr - ma->base);
	CopyOut (ret_phys, &phys, sizeof (vm_addr));
	
	if (seg->wired == TRUE)
	    return memoryErr;
		
	DisablePreemption();
	
	if (seg->busy == TRUE)
		Sleep (&vm_rendez);
	
	seg->busy = TRUE;
	seg->wired = TRUE;
	return 0;
}




/*
 * Unwires a segment of memory previously flagged by WireMem()
 */

SYSCALL int UnwireMem (vm_addr addr)
{
	struct MemArea *ma;
	struct Process *current;
	struct Segment *seg;

	current = GetCurrentProcess();

	if (!(current->flags & PROCF_ALLOW_IO))
	    return privilegeErr;
		
	addr = ALIGN_DOWN (addr, PAGE_SIZE);
		
	if ((ma = MemAreaFind (addr)) == NULL || ma->owner_process != current
	    || MEM_TYPE(ma->flags) != MEM_ALLOC)
	{
		return memoryErr;
    }
    
	seg = SegmentFind (ma->physical_addr);

	if (seg->wired == FALSE)
	    return memoryErr;

	DisablePreemption();
	
	seg->busy = FALSE;
	seg->wired = FALSE;
	Wakeup (&vm_rendez);
	return 0;
}
















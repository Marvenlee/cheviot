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
    Map an area of memory and unmap any existing mapping
 */

SYSCALL vm_size VirtualAlloc (vm_addr addr, vm_size len, bits32_t flags)
{
	struct Process *current;
	struct AddressSpace *as;
	vm_size nbytes_mapped;
    vm_addr va;
    	
	current = GetCurrentProcess();
    as = &current->as;

    addr = ALIGN_DOWN (addr, PAGE_SIZE);
    len = ALIGN_UP (len, PAGE_SIZE);
    flags = (flags & ~VM_SYSTEM_MASK) | MEM_ALLOC;

    DisablePreemption();
    nbytes_mapped = 0;
    
    for (va = addr; va < addr + len; va += PAGE_SIZE)
    {
        if (PmapEnter (as, va, 0, flags) != 0)
        {
            break;
        }
        
        nbytes_mapped += PAGE_SIZE;
    
//        PreeemptionPoint();
    }
    
    return nbytes_mapped;
}



/*
 * Maps an area of physical memory such as IO device or framebuffer into the
 * address space of the calling process.
 */

SYSCALL vm_size VirtualAllocPhys (vm_addr addr, vm_size len, bits32_t flags, vm_addr paddr)
{
	struct Process *current;
	struct AddressSpace *as;
    vm_addr va;
    vm_addr pa;
    vm_size nbytes_mapped;
    
	current = GetCurrentProcess();
	as = &current->as;
	
	addr = ALIGN_DOWN (addr, PAGE_SIZE);
	paddr = ALIGN_DOWN (paddr, PAGE_SIZE);
	len = ALIGN_UP (len, PAGE_SIZE);
	flags = (flags & ~VM_SYSTEM_MASK) | MEM_PHYS;

	if (!(current->flags & PROCF_ALLOW_IO))
    {
    	return 0;
    }

    DisablePreemption();
    nbytes_mapped = 0;
    
    for (va = addr, pa = paddr; va < addr + len; va += PAGE_SIZE, pa += PAGE_SIZE)
    {
        if (PmapEnter (as, va, pa, flags) != 0)
        {
            break;
        }
        
        nbytes_mapped += PAGE_SIZE;
    }
    
    return nbytes_mapped;
}



/*
 *
 */

SYSCALL vm_size VirtualFree (vm_addr addr, vm_size size)
{
	struct Process *current;
	struct AddressSpace *as;
	vm_addr va;
	vm_size nbytes_freed;
	
	current = GetCurrentProcess();
	as = &current->as;
	
	addr = ALIGN_DOWN (addr, PAGE_SIZE);
	size = ALIGN_UP (size, PAGE_SIZE);
	    
    nbytes_freed = 0;
        
    for (va = addr; va < addr + size; va += PAGE_SIZE)
    {
        if (PmapRemove (as, va) == 0)
        {
            nbytes_freed += PAGE_SIZE;
        }
	}
	
	return nbytes_freed;
}




/*
 * Changes the read/write/execute protection attributes of a virtual
 * memory segment pointed to by 'addr'..
 *
 * A bit harder, as we only want to change segment permissions for a range,  no PADDR changes
 
 */

SYSCALL vm_size VirtualProtect (vm_addr addr, vm_size size, bits32_t flags)
{
	struct Process *current;
	struct AddressSpace *as;
	vm_addr va;
	vm_size nbytes_protected = 0;
	
	current = GetCurrentProcess();
	as = &current->as;
	
	addr = ALIGN_DOWN (addr, PAGE_SIZE);
	size = ALIGN_UP (size, PAGE_SIZE);

//    new_flags = flags | MAP_FIXED | PROT_CHANGE;
    
    // TODO:

    for (va = addr; va < addr + size; va += PAGE_SIZE)
    {
        if (PmapProtect (as, va, flags) != 0)
        {
            break;
        }
        
        nbytes_protected += PAGE_SIZE;
	}


	return nbytes_protected;
}




/*
 *
 */

int ForkAddressSpace (struct AddressSpace *new_as, struct AddressSpace *old_as)
{
	if (PmapInit (new_as) != 0)
	{
		return resourceErr;
	}

    if (PmapFork(new_as, old_as) != 0)
    {
        PmapDestroy (new_as);
        return resourceErr;
    }

	return 0;
}





void FreeAddressSpace (struct AddressSpace *as)
{
}





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


/**

 */
int PageFault (vm_addr addr, bits32_t access)
{
    struct Process *current;
	uint32 page_flags;
	uint32 paddr;
	vm_addr src_kva;
	vm_addr dst_kva;
	struct Pageframe *pf;

    current = GetCurrentProcess();
	
    KLog ("PageFault addr: %08x  access: %08x", addr, access);		    
    
    addr = ALIGN_DOWN (addr, PAGE_SIZE);
    
    // Currently no support for lazy mapping of areas of memory, so if
    // not currently mapped exit process.

    if (PmapExtract (&current->as, addr, &paddr, &page_flags) != 0)
    {
        // Page is not present
        current->task_state.flags |= TSF_EXCEPTION;
        Exit (memoryErr);
    }
    
    
    DisablePreemption();
    
    if ((page_flags & MEM_MASK) == MEM_PHYS)
	{
        current->task_state.flags |= TSF_EXCEPTION;
        Exit (memoryErr);
	}
    else if ((page_flags & MEM_MASK) == MEM_ALLOC)
	{
	    if (access == PROT_WRITE && page_flags == MAP_COW)
	    {	    
	        if ((pf = AllocPageframe(PAGE_SIZE)) == NULL)        
            {
                current->task_state.flags |= TSF_EXCEPTION;
	            Exit (memoryErr);
            }
	        
	        if (PmapRemove (&current->as, addr) != 0)
	        {
                current->task_state.flags |= TSF_EXCEPTION;
    	        Exit (memoryErr);
	        }
	        
	        src_kva = PmapPaToVa (paddr);
	        dst_kva = PmapPaToVa (pf->physical_addr);
	        MemCpy ((void *)dst_kva, (void *)src_kva, PAGE_SIZE);
	        page_flags = (page_flags | PROT_WRITE) & ~MAP_COW;

	        if (PmapEnter (&current->as, addr, pf->physical_addr, page_flags) != 0)
	        {
                current->task_state.flags |= TSF_EXCEPTION;
    	        Exit (memoryErr);
	        }	    
	    }
	    else
	    {
            current->task_state.flags |= TSF_EXCEPTION;
	        Exit (memoryErr);
	    }
	}
	else
	{
        current->task_state.flags |= TSF_EXCEPTION;
		Exit (memoryErr);
	}    
    
	return 0;
}


/**
    Allocate a 4k, 16k or 64k page, splitting larger slabs into smaller sizes if needed.
    Could eventually do some page movement to make contiguous memory.    
 */
 struct Pageframe *AllocPageframe (vm_size size)
{
	struct Pageframe *head = NULL;
    int t;
    
    if (size <= 4096)
    {
        head = LIST_HEAD (&free_4k_pf_list);        
        LIST_REM_HEAD (&free_4k_pf_list, link);  
    }

    if (head == NULL && size <= 16384)
    {
        head = LIST_HEAD (&free_16k_pf_list);  
        LIST_REM_HEAD (&free_16k_pf_list, link);  

    }
    
    if (head == NULL && size <= 65536)
    {
        head = LIST_HEAD (&free_64k_pf_list);
        LIST_REM_HEAD (&free_64k_pf_list, link);  
    }
    
    if (head == NULL)
        return NULL;
        
    
    // Split 64k slabs if needed into 4k or 16k allocations. 
    
    if (head->size == 65536 && size == 16384)
    {
        for (t=3; t>0; t--)
        {
            LIST_ADD_HEAD (&free_16k_pf_list, head + t*16384/PAGE_SIZE, link);
        }
    }
    else if (head->size == 65536 && size == 4096)
    {
        for (t=15; t>0; t--)
        {
            LIST_ADD_HEAD (&free_4k_pf_list, head + t*4096/PAGE_SIZE, link);
        }
    }
    
    head->size = size;
    head->flags = PGF_INUSE;
    head->reference_cnt = 0;             
    PmapPageframeInit (&head->pmap_pageframe);
    return head;    
}


/**
    Free page of memory and coalesce into 16k or 64k slabs    
 */
void FreePageframe (struct Pageframe *pf)
{
    pageframe_list_t *free_list;
    int in_use = 0;
    uint32 base_idx;
    uint32 stride;
    int t;
    
    if (pf->size == 65536)
    {
        LIST_ADD_TAIL (&free_64k_pf_list, pf, link);
        pf->flags = 0;            
    }
    else 
    {
        if (pf->size == 16384)
        {
            free_list = &free_16k_pf_list;
        }
        else
        {
            free_list = &free_4k_pf_list;    
        }

        LIST_ADD_TAIL (free_list, pf, link);            
                    
        pf->flags = 0;            
        pf->reference_cnt = 0;
        
        base_idx = ALIGN_DOWN ((pf - pageframe_table), pf->size/PAGE_SIZE);
        stride = pf->size/PAGE_SIZE;
                
        for (t=base_idx; t< base_idx + (65536/pf->size) * stride; t += stride)
        {
            
            if (pageframe_table[t].flags & PGF_INUSE)
            {
                in_use ++;
            }
        }

        if (in_use == 0)
        {
            for (t=base_idx; t< base_idx + (65536/pf->size) * stride; t += stride)
            {
                LIST_REM_ENTRY (free_list, &pageframe_table[t], link)
            }

            LIST_ADD_TAIL (free_list, &pageframe_table[base_idx], link)
        }
    }
}




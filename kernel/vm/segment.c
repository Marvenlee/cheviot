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
 * of PhysicalSegment structures.
 */
 
#include <kernel/types.h>
#include <kernel/lists.h>
#include <kernel/vm.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>




/*
 * Coalesces free physical memory physsegs but does not compact them.
 * Shrinks the size of the physseg_table array that manages physical memory.
 *
 * Called by a kernel task to periodically coalesce memory.  Algorithm could
 * be improved to leave a certain number of common sizes avaiable such as
 * 256k, 64k and 4k.
 */
 
SYSCALL int CoalescePhysicalMem (void)
{
    int t, s;
    struct Process *current;
    

    current = GetCurrentProcess();
    
    if (!(current->flags & PROCF_SYSTEMTASK))
        return privilegeErr;

    DisablePreemption();

    for (t = 0, s = 0; t < physseg_cnt; t++)
    {
        if (s > 0 && (physseg_table[s-1].type == SEG_TYPE_FREE && physseg_table[t].type == SEG_TYPE_FREE))
        {
            physseg_table[s-1].ceiling      = physseg_table[t].ceiling;
        }
        else
        {
            physseg_table[s].base           = physseg_table[t].base;
            physseg_table[s].ceiling        = physseg_table[t].ceiling;
            physseg_table[s].type           = physseg_table[t].type;
            physseg_table[s].virtual_addr   = physseg_table[t].virtual_addr;
            s++;
        }
    }

    physseg_cnt = s;
    return 0;
}






/*
 * Compacts physical memory.  moves segments downwards in
 * physical memory unless they are locked by the WireMem() system
 * call.
 *
 * To be called by a kernel task running with SYSTEM privileges
 * that periodically coalesces and compacts memory based on memory
 * demands.
 *
 * The kernel task needs to have physical memory mapped or the page
 * fault handler needs to special case the handling of page faults
 * that it generates.
 *
 * The kernel thread has to make this system call and not access
 * the data structures directly in order to lock access to kernel
 * data structures, make use of preemption/restartable system calls etc.
 */

SYSCALL vm_addr CompactPhysicalMem (vm_addr addr)
{
    return addr;
}














/*
 * Searches for and returns a suitably sized free physical memory segment of
 * the desired size and type. Inserts entries into the physseg_table array
 * if needed
 */

struct PhysicalSegment *PhysicalSegmentCreate (vm_offset addr, vm_size size, int type)
{
    struct PhysicalSegment *ps;
    vm_addr base, ceiling;
    bits32_t flags;
    int t;


    // FIXME: AWOOGA: Why set flags to zero??????

    flags = 0;
     
    KASSERT (physseg_cnt < (max_physseg - 3));   
    
    if ((ps = PhysicalSegmentBestFit (size, flags, &addr)) == NULL)
        return NULL;
    
    
    KASSERT (ps->base <= addr && addr+size <= ps->ceiling);
    
    t = ps - physseg_table;

    if (ps->base == addr && ps->ceiling == addr + size)
    {
        /* Perfect fit, initialize region */
        
        ps->base = addr;
        ps->ceiling = addr + size;
        ps->type = type;
    }
    else if (ps->base < addr && ps->ceiling > addr + size)
    {
        /* In the middle, between two parts */
    
        base = ps->base;
        ceiling = ps->ceiling;
    
        PhysicalSegmentInsert (t, 2);
                
        ps->base = base;
        ps->ceiling = addr;
        ps->type = SEG_TYPE_FREE;
        ps++;
        
        ps->base = addr;
        ps->ceiling = addr + size;
        ps->type = type;
        ps++;
        
        ps->base = addr + size;
        ps->ceiling = ceiling;
        ps->type = SEG_TYPE_FREE;
        ps --;     // Return the middle ps
        
    }
    else if (ps->base == addr && ps->ceiling > addr + size)
    {
        /* Starts at bottom of area */
    
        base = ps->base;
        ceiling = ps->ceiling;
    
        PhysicalSegmentInsert (t, 1);
        
        ps->base = addr;
        ps->ceiling = addr + size;
        ps->type = type;
        ps++;
        
        ps->base = addr + size;
        ps->ceiling = ceiling;
        ps->type = SEG_TYPE_FREE;
        ps --;         // Return the first ps
    }
    else
    {
        /* Starts at top of area */
        
        base = ps->base;
        ceiling = ps->ceiling;
    
        PhysicalSegmentInsert (t, 1);
        
        ps->base = base;
        ps->ceiling = addr;
        ps->type = SEG_TYPE_FREE;
        
        ps++;
        ps->base = addr;
        ps->ceiling = addr + size;
        ps->type = type;
        
        // Return the second ps
    }
    
    
    return ps;
}




/*
 * Marks a PhysicalSegment as free.  Does not coalesce free physsegs.
 * Coalescing is done by a seperate CoalescePhysicalMem() system call.
 */
 
void PhysicalSegmentDelete (struct PhysicalSegment *ps)
{
    ps->type = SEG_TYPE_FREE;
}




/*
 * Inserts 'cnt' empty physseg_table entries after physseg_table entry 'index' 
 * Used by PhysicalSegmentCreate() if a perfectly sized physseg could not be found.
 * This inserts free entries in the physseg_table so that PhysicalSegmentCreate() can
 * carve up a larger free physseg into the desired size plus the leftovers.
 */
 
void PhysicalSegmentInsert (int index, int cnt)
{
    int t;
    
    for (t = physseg_cnt - 1 + cnt; t >= (index + cnt); t--)
    {
        physseg_table[t].base           = physseg_table[t-cnt].base;
        physseg_table[t].ceiling        = physseg_table[t-cnt].ceiling;
        physseg_table[t].type           = physseg_table[t-cnt].type;
        physseg_table[t].virtual_addr   = physseg_table[t-cnt].virtual_addr;
    }
    
    physseg_cnt += cnt;
}




/*
 *
 */

struct PhysicalSegment *PhysicalSegmentFind (vm_addr addr)
{
    int low = 0;
    int high = physseg_cnt - 1;
    int mid;    
    
    
    while (low <= high)
    {
        mid = low + ((high - low) / 2);
        
        if (addr >= physseg_table[mid].ceiling)
            low = mid + 1;
        else if (addr < physseg_table[mid].base)
            high = mid - 1;
        else
            return &physseg_table[mid];
    }

    return NULL;
}




/*
 * Performs a best fit search for a physseg of physical memory.
 * returns the physseg and the physical address of where it physseg
 * to be returned to the user should start.  This may be different
 * from the base address of the physseg if alignment options
 * are added to the code.
 * PhysicalSegmentCreate() then carves this physseg up if it is larger than
 * the requested size.
 */

struct PhysicalSegment *PhysicalSegmentBestFit (vm_size size, uint32 flags, vm_addr *ret_addr)
{
    int i;
    vm_size ps_size;
    vm_size best_size = VM_USER_CEILING - VM_USER_BASE;
    int best_idx = -1;
    bool found = FALSE;
    
    
    
    // Might want to support NOX64 (same for compaction)

    for (i = 0; i < physseg_cnt; i++)
    {
        if (physseg_table[i].type != SEG_TYPE_FREE)
            continue;
        
        if (found == FALSE)
            found = TRUE;
    
        ps_size = physseg_table[i].ceiling - physseg_table[i].base;
    
        if (ps_size > size && ps_size < best_size)
        {
            best_size = ps_size;
            best_idx = i;
            
        }
        else if (ps_size == size)
        {
            *ret_addr = physseg_table[i].base;
            return &physseg_table[i];
        }
        
    }
    
    if (best_idx == -1)
        return NULL;

    *ret_addr = physseg_table[best_idx].base;
    return &physseg_table[best_idx];
}









































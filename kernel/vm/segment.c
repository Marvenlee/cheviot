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
 * Coalesces free physical memory psegs but does not compact them.
 * Shrinks the size of the pseg_table array that manages physical memory.
 *
 * Called periodically by the kernel's VM daemon.
 * 
 * Algorithm could be improved to leave a certain number of common sizes
 * available such as 256k, 64k and 4k.
 */
 
SYSCALL int CoalescePhysicalMem (void)
{
    int t, s;
    struct Process *current;
    

    current = GetCurrentProcess();
    
    if (!(current->flags & PROCF_DAEMON))
        return privilegeErr;

    DisablePreemption();

    for (t = 0, s = 0; t < pseg_cnt; t++)
    {
        if (s > 0 && (pseg_table[s-1].type == SEG_TYPE_FREE && pseg_table[t].type == SEG_TYPE_FREE))
        {
            pseg_table[s-1].ceiling      = pseg_table[t].ceiling;
        }
        else
        {
            pseg_table[s].base           = pseg_table[t].base;
            pseg_table[s].ceiling        = pseg_table[t].ceiling;
            pseg_table[s].type           = pseg_table[t].type;
            pseg_table[s].virtual_addr   = pseg_table[t].virtual_addr;
            s++;
        }
    }

    pseg_cnt = s;
    return 0;
}






/*
 * Compacts physical memory.  moves segments downwards in
 * physical memory unless they are locked by the WireMem() system
 * call.
 *
 * Called periodically by the kernel's VM daemon.
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
 * the desired size and type. Inserts entries into the pseg_table array
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
     
    KASSERT (pseg_cnt < (max_pseg - 3));   
    
    if ((ps = PhysicalSegmentBestFit (size, flags, &addr)) == NULL)
        return NULL;
    
    
    KASSERT (ps->base <= addr && addr+size <= ps->ceiling);
    
    t = ps - pseg_table;

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
 * Marks a PhysicalSegment as free.  Does not coalesce free psegs.
 * Coalescing is done by a seperate CoalescePhysicalMem() system call.
 */
 
void PhysicalSegmentDelete (struct PhysicalSegment *ps)
{
    ps->type = SEG_TYPE_FREE;
}




/*
 * Inserts 'cnt' empty pseg_table entries after pseg_table entry 'index' 
 * Used by PhysicalSegmentCreate() if a perfectly sized pseg could not be found.
 * This inserts free entries in the pseg_table so that PhysicalSegmentCreate() can
 * carve up a larger free pseg into the desired size plus the leftovers.
 */
 
void PhysicalSegmentInsert (int index, int cnt)
{
    int t;
    
    for (t = pseg_cnt - 1 + cnt; t >= (index + cnt); t--)
    {
        pseg_table[t].base           = pseg_table[t-cnt].base;
        pseg_table[t].ceiling        = pseg_table[t-cnt].ceiling;
        pseg_table[t].type           = pseg_table[t-cnt].type;
        pseg_table[t].virtual_addr   = pseg_table[t-cnt].virtual_addr;
    }
    
    pseg_cnt += cnt;
}




/*
 *
 */

struct PhysicalSegment *PhysicalSegmentFind (vm_addr addr)
{
    int low = 0;
    int high = pseg_cnt - 1;
    int mid;    
    
    
    while (low <= high)
    {
        mid = low + ((high - low) / 2);
        
        if (addr >= pseg_table[mid].ceiling)
            low = mid + 1;
        else if (addr < pseg_table[mid].base)
            high = mid - 1;
        else
            return &pseg_table[mid];
    }

    return NULL;
}




/*
 * Performs a best fit search for a pseg of physical memory.
 * returns the pseg and the physical address of where it pseg
 * to be returned to the user should start.  This may be different
 * from the base address of the pseg if alignment options
 * are added to the code.
 * PhysicalSegmentCreate() then carves this pseg up if it is larger than
 * the requested size.
 *
 * Perhaps we could allocate at the top of the heap, if there isn't enough room
 * then compact.
 */

struct PhysicalSegment *PhysicalSegmentBestFit (vm_size size, uint32 flags, vm_addr *ret_addr)
{
    int i;
    vm_size ps_size;
    vm_size best_size = VM_USER_CEILING - VM_USER_BASE;
    int best_idx = -1;
    vm_addr new_base;
    vm_size new_size;
    
    
    for (i = 0; i < pseg_cnt; i++)
    {
        if (pseg_table[i].type != SEG_TYPE_FREE)
            continue;
            
        ps_size = pseg_table[i].ceiling - pseg_table[i].base;
    
        if (ps_size > size && ps_size < best_size)
        {
            best_size = ps_size;
            best_idx = i;
            
        }
        else if (ps_size == size)
        {
            *ret_addr = pseg_table[i].base;
            return &pseg_table[i];
        }
        
    }
    
    if (best_idx == -1)
        return NULL;


    if (best_size >= LARGE_PAGE_SIZE)
    {
        new_base = ALIGN_UP (pseg_table[i].base, LARGE_PAGE_SIZE);
        
        if (new_base < pseg_table[i].ceiling)
        {
            new_size = pseg_table[i].ceiling - new_base;
        
            if (new_size >= size)
            {
                *ret_addr = new_base;
                return &pseg_table[best_idx];
            }
        }
    }

    *ret_addr = pseg_table[best_idx].base;
    return &pseg_table[best_idx];
}









































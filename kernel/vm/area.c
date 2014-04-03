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

#include <kernel/types.h>
#include <kernel/vm.h>
#include <kernel/lists.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/utility.h>
#include <kernel/globals.h>




/*
 * Coalesces free virtual memory areas and compacts the memarea array.
 * 
 * As 
 * **************** DOUBLE BUFFER segment and memarea tables on alloc/coalescing
 * ******** Committing transaction just changes pointer to new tables.
 *
 * May want separate FindSuitableSeg(), FindSuitableMemArea(), then a function to
 * coalesce/enlarge if necessary.
 */
 
SYSCALL int CoalesceVirtualMem (void)
{
    int t, s;
    struct Process *current;
    

    current = GetCurrentProcess();
    
    if (!(current->flags & PROCF_EXECUTIVE))
        return privilegeErr;
    
    DisablePreemption();
    
    for (t = 0, s = 0; t < memarea_cnt; t++)
    {
        if (s > 0 && (MEM_TYPE(memarea_table[s-1].flags) == MEM_FREE
                                 && MEM_TYPE(memarea_table[t].flags) == MEM_FREE))
        {
            memarea_table[s-1].ceiling = memarea_table[t].ceiling;
        }
        else
        {
            memarea_table[s].base          = memarea_table[t].base;
            memarea_table[s].ceiling       = memarea_table[t].ceiling;
            memarea_table[s].physical_addr = memarea_table[t].physical_addr;
            memarea_table[s].flags         = memarea_table[t].flags;
            memarea_table[s].owner_process = memarea_table[t].owner_process;
            memarea_table[s].msg_handle    = memarea_table[t].msg_handle;
            s++;
        }
    }

    memarea_cnt = s;
    return 0;
}




/*
 * MemAreaCreate();
 */

struct MemArea *MemAreaCreate (vm_offset addr, vm_size size, bits32_t flags)
{
    struct MemArea *ma;
    vm_addr base, ceiling;
    int t;

    KASSERT (memarea_cnt < (max_memarea - 3));
    
    if (flags & MAP_FIXED)
    {   
        if ((ma = MemAreaFind(addr)) == NULL)
        {
            return NULL;
        }
            
        if (ma->ceiling - addr < size || MEM_TYPE(ma->flags) != MEM_FREE)
        {
            return NULL;
        }
    }
    else if ((ma = MemAreaAlloc (size, flags, &addr)) == NULL)
    {
        return NULL;
    }
    
    KASSERT (MEM_TYPE(ma->flags) == MEM_FREE);
    KASSERT (ma->base <= addr && addr+size <= ma->ceiling);
    
    t = ma - memarea_table;

    if (ma->base == addr && ma->ceiling == addr + size)
    {
        /* Perfect fit, initialize region */
        
        ma->base = addr;
        ma->ceiling = addr + size;
        ma->flags = flags;
        ma->owner_process = NULL;
        ma->msg_handle = -1;
    }
    else if (ma->base < addr && ma->ceiling > addr + size)
    {
        /* In the middle, between two parts */
    
        base = ma->base;
        ceiling = ma->ceiling;
    
        MemAreaInsert (t, 2);
                
        ma->base = base;
        ma->ceiling = addr;
        ma->flags = MEM_FREE;
        ma++;
        
        ma->base = addr;
        ma->ceiling = addr + size;
        ma->flags = flags;
        ma->owner_process = NULL;
        ma->msg_handle = -1;
        ma++;
        
        ma->base = addr + size;
        ma->ceiling = ceiling;
        ma->flags = MEM_FREE;
        ma --;      // Return the middle ma
        
    }
    else if (ma->base == addr && ma->ceiling > addr + size)
    {
        /* Starts at bottom of area */
    
        base = ma->base;
        ceiling = ma->ceiling;
    
        MemAreaInsert (t, 1);
        
        ma->base = addr;
        ma->ceiling = addr + size;
        ma->flags = flags;
        ma->owner_process = NULL;
        ma->msg_handle = -1;
        ma++;
        
        ma->base = addr + size;
        ma->ceiling = ceiling;
        ma->flags = MEM_FREE;
        ma --;      // Return the first ma
    }
    else
    {
        /* Starts at top of area */
        
        base = ma->base;
        ceiling = ma->ceiling;
    
        MemAreaInsert (t, 1);
        
        ma->base = base;
        ma->ceiling = addr;
        ma->flags = MEM_FREE;
        
        ma++;
        ma->base = addr;
        ma->ceiling = addr + size;
        ma->flags = flags;
        ma->owner_process = NULL;
        ma->msg_handle = -1;
        
        // Return the second ma
    }
    
    
    return ma;
}




/*
 * MemAreaDelete();
 */
 
void MemAreaDelete (struct MemArea *ma)
{
    KASSERT (MEM_TYPE(ma->flags) != MEM_FREE);
        
    ma->flags = (ma->flags & ~MEM_MASK) | MEM_FREE;
}




/*
 * MemAreaInsert();
 *


 */
 
void MemAreaInsert (int index, int cnt)
{
    int t;
    
    for (t = memarea_cnt - 1 + cnt; t >= (index + cnt); t--)
    {
        memarea_table[t].base          = memarea_table[t-cnt].base;
        memarea_table[t].ceiling       = memarea_table[t-cnt].ceiling;
        memarea_table[t].physical_addr = memarea_table[t-cnt].physical_addr;
        memarea_table[t].flags         = memarea_table[t-cnt].flags;
        memarea_table[t].owner_process = memarea_table[t-cnt].owner_process;
        memarea_table[t].msg_handle    = memarea_table[t-cnt].msg_handle;
    }
    
    memarea_cnt += cnt;
}




/*
 *
 */

struct MemArea *MemAreaFind (vm_addr addr)
{
    int low = 0;
    int high = memarea_cnt - 1;
    int mid;    
        
        
    while (low <= high)
    {
        mid = low + ((high - low) / 2);
        
        if (addr >= memarea_table[mid].ceiling)
            low = mid + 1;
        else if (addr < memarea_table[mid].base)
            high = mid - 1;
        else
            return &memarea_table[mid];
    }

    return NULL;
}




/*
 *
 */

struct MemArea *MemAreaAlloc (vm_size size, uint32 flags, vm_addr *ret_addr)
{
    int i;
    vm_size ma_size;
    vm_size best_size = VM_USER_CEILING - VM_USER_BASE;
    int best_idx = -1;
    

    for (i = 0; i < memarea_cnt; i++)
    {
        if (MEM_TYPE(memarea_table[i].flags) == MEM_FREE)
        {
            ma_size = memarea_table[i].ceiling - memarea_table[i].base;
        
            if (ma_size > size && ma_size < best_size)
            {
                best_size = ma_size;
                best_idx = i;
            }
            else if (ma_size == size)
            {
                *ret_addr = memarea_table[i].base;
                return &memarea_table[i];
            }
        }           
    }
    
    if (best_idx == -1)
        return NULL;

    *ret_addr = memarea_table[best_idx].base;
    return &memarea_table[best_idx];
}






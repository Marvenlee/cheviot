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
#include <kernel/lists.h>
#include <kernel/arch.h>
#include <kernel/vm.h>
#include <kernel/proc.h>
#include <kernel/utility.h>
#include <kernel/error.h>
#include <kernel/dbg.h>
#include <kernel/globals.h>



/*
 * Performs any initialization of the CPU-independent part of the memory
 * management system for a newly created process.
 * At present we do nothing.
 */

void InitAddressSpace (struct Process *proc)
{
}




/*
 * Free all memory in the current process upon process termination.  Called in
 * the context of the exiting process.
 */

void FreeAddressSpace (void)
{
    struct Process *current;
    int t;
  

    current = GetCurrentProcess();
    
    DisablePreemption();
        
    for (t= 0; t < seg_cnt; t++)
    {   
        // FIXME:  If segment is cached and belongs to proces we need to free it.
        // May have to go through separate search of pseg table?
        
        // FIXME: Store current position, to avoid sleep/wakeup starting
        // from beginning.
        
        if (seg_table[t].owner == current
            && MEM_TYPE (seg_table[t].flags) == MEM_ALLOC)
        {
            VirtualFree (seg_table[t].base);
        }    
    }
}







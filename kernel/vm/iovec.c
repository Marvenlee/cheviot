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
#include <limits.h>



/*
 * Used by PutMsg() and Spawn() to copy in an array of pointers to memory
 * segments pointed to by user_iovtable. These segments will either be
 * passed in a message or used to create a new process. This function finds
 * the MemArea structure for each segment and initializes the array matable
 * to point to each MemArea.
 */

int CopyInIOVs (struct MemArea **matable, vm_addr *user_iovtable, int niov)
{
    struct MemArea *ma;
    struct Segment *seg;
    vm_addr iovtable[MAX_IOV_SEGMENTS];
    int t, u;
    struct Process *current;
    
    
    
    if (niov < 1 || niov >= MAX_IOV_SEGMENTS)
        return paramErr;
    
    CopyIn (iovtable, user_iovtable, niov * sizeof (vm_addr));
    current = GetCurrentProcess();

    for (t=0; t< niov; t++)
    {
        ma = MemAreaFind (iovtable[t]);
        
        if (ma == NULL || ma->owner_process != current || (ma->flags & MEM_MASK) != MEM_ALLOC)
            return memoryErr;
            
        if ((ma->ceiling - ma->base) > INT_MAX)
            return memoryErr;   
        
        seg = SegmentFind (ma->physical_addr);
        
        if (seg->busy == TRUE)
            Sleep(&vm_rendez);
        
        for (u=0; u<t; u++)
        {
            if (ma == matable[u])
                return paramErr;
        }
        
        matable[t] = ma;
    }
    
    return 0;
}   


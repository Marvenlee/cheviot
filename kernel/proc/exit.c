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
 * System call to exit a process.
 */

#include <kernel/types.h>
#include <kernel/proc.h>
#include <kernel/timer.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/arch.h>
#include <kernel/globals.h>



/*
 * System call to exit a process.
 */

SYSCALL void Exit (int status)
{
    int t;
    struct Process *current;
    
    current = GetCurrentProcess();
    
    current->task_state.flags |= TSF_EXIT;
    
    DisablePreemption();
    
    for (t=0; t < max_handle; t++)
        CloseHandle(t); 

    PmapRemoveAll(&current->as);
                
    DisableInterrupts();
    current->state = PROC_STATE_ZOMBIE; 

    DoRaiseEvent (current->handle);

    SchedUnready (current);
    EnableInterrupts();
    
    // Kernel exit path will reschedule threads.    
}




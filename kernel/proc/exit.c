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
 * System call to exit a process.  The actual work of exiting a process is
 * performed at the end of a system call in the KernelExit function.
 */

SYSCALL void Exit (int status)
{
    struct Process *current;
    
    current = GetCurrentProcess();
    
    current->exit_status = status;
    current->task_state.flags |= TSF_EXIT;
}





/*
 * DoExit();
 *
 * Called by the KernelExit routine.to terminate the process.
 * Exits a process and saves the exit status in the process structure
 * for Join to retrieve.
 *
 * Currently we scan through the entire handle table with preemption
 * disabled and it continues to be disabled for the rest of DoExit().
 * Maintaining a current_exit_handle field in the Process structure
 * would allow preemption points inside that loop.
 *
 * FreeAddressSpace() could also have preemption points, again maintaining
 * a virtual address pointer of the next segment to check.
 */

void DoExit (int status)
{
    int t;
    struct Process *current;
    
    current = GetCurrentProcess();
    
    current->task_state.flags |= TSF_EXIT;
    
    DisablePreemption();
    
    for (t=0; t < max_handle; t++)
        CloseHandle(t); 

    FreeAddressSpace();             
    
    ClosePendingHandles();    
                
    DisableInterrupts();
    current->state = PROC_STATE_ZOMBIE; 

    DoRaiseEvent (current->handle);

    SchedUnready (current);
    EnableInterrupts();
    
    __KernelExit();         // FIXME: Use pragma/attribute to indicate
                            // This __KernelExit doesn't return
}




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
 * Event handling. Raising and receiving notification of events relating
 * to handles such as process termination, messages arriving on a Channel,
 * the state of a Condition changing, the opposite end of a Channel
 * or Condition closing, timers expiring and interrupts being raised.
 *
 * WaitFor() is a system call that puts a process to sleep until an event
 * is raised by one of the aforementioned events.  A specific handle can be
 * chosen to wait for or all handles belonging to a process.
 */
  
#include <kernel/types.h>
#include <kernel/proc.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>





/*
 * Checks to see if an event has been raised on a specific handle
 */
 
SYSCALL int CheckFor (int h)
{
    struct Process *current;
    
    
    DisablePreemption();
    
    current = GetCurrentProcess();  

    if (h < 0 || h >= max_handle)
        return paramErr;

    if (handle_table[h].type != HANDLE_TYPE_FREE
        && handle_table[h].owner == current
        && handle_table[h].pending == 1)
    {
        handle_table[h].pending = 0;
        LIST_REM_ENTRY (&current->pending_handle_list, &handle_table[h], link);
        return h;
    }
    else
    {
        return paramErr;
    }
}




/*
 * Waits for an event to occur on a specific handle or if 'h' is set to -1 it
 * waits for any object owned by the process to raise an event.  If an event
 * has already been raised the function returns immediately.
 */

SYSCALL int WaitFor (int h)
{
    struct Process *current;
    struct Handle *hp;

        
    current = GetCurrentProcess();

    if (h < -1 || h >= max_handle)
        return paramErr;

    if ((current->task_state.flags & TSF_KILL) != 0)
    {
        __KernelExit();
        return undefinedErr;
    }
        

    DisablePreemption();
    
    if (h == -1)
    {
        hp = LIST_HEAD (&current->pending_handle_list);

        if (hp == NULL)
        {
            current->waiting_for = -1;
            Sleep (&current->waitfor_rendez);
        }
    
        hp->pending = 0;

        LIST_REM_HEAD (&current->pending_handle_list, link);
        h = hp - handle_table;
        return h;
    }
    else if (handle_table[h].owner == current)
    {
        if (handle_table[h].pending == 0)
        {
            current->waiting_for = h;       
            Sleep (&current->waitfor_rendez);
        }
        
        hp = &handle_table[h];
        hp->pending = 0;
        
        LIST_REM_ENTRY (&current->pending_handle_list, hp, link);
        return h;
    }
    
    return handleErr;
}




/*
 * Raises an event.  Common code used by various parts of the kernel to raise
 * an event on a specific handle.
 */

void DoRaiseEvent (int h)
{   
    struct Process *proc;

    KASSERT (inkernel_lock == 1);
    KASSERT (h >= 0 || h < max_handle)

    
    proc = handle_table[h].owner;
    
    if (handle_table[h].pending == 0)
    {
        LIST_ADD_TAIL (&proc->pending_handle_list, &handle_table[h], link);
        handle_table[h].pending = 1;
    }
    
    if (proc->waiting_for == h || proc->waiting_for == -1)
        Wakeup (&proc->waitfor_rendez);
}




/*
 * Clears an event on a specific handle.  Used when closing handles to clear a
 * pending event.
 */

void DoClearEvent (struct Process *proc, int h)
{
    KASSERT (inkernel_lock == 1);
    
    
    if (h < 0 || h >= max_handle)
        return;
        
    if (handle_table[h].type == HANDLE_TYPE_FREE || handle_table[h].owner != proc)
        return;

    if (handle_table[h].pending == 1)
        LIST_REM_ENTRY (&proc->pending_handle_list, &handle_table[h], link);

    handle_table[h].pending = 0;
}

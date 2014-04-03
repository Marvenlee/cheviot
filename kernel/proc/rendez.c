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
 * Rendez (or condition variable) functions used within the kernel to
 * put a process to sleep until it is woken by another process.  When a
 * process wakes up, it either restarts the system call or the instruction
 * afterwards if Sleep() was called in KernelExit.
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
 * Initializes a Rendez condition variable.  Used in the kernel and
 * in particular struct Process to send a process to sleep waiting on
 * a resource or event and to wakeup all processes waiting on the
 * Rendez.
 */
 
void InitRendez (struct Rendez *rendez)
{
    LIST_INIT (&rendez->process_list);
}




/*
 * Puts a process to sleep waiting on the rendez condition variable.
 * Upon waking up the process will restart the system call from the
 * beginning.
 */

void Sleep (struct Rendez *rendez)
{
    struct Process *current;
    
    current = GetCurrentProcess();
    
    DisablePreemption();
    
    current->state = PROC_STATE_SLEEP;
    current->sleeping_on = rendez;
    LIST_ADD_TAIL (&rendez->process_list, current, rendez_link);
    SchedUnready (current);
    __KernelExit();
}




/*
 * Wakes up all processes sleeping on the rendez condition variable.
 * System calls will restart from the beginning once woken up.
 */

void Wakeup (struct Rendez *rendez)
{
    struct Process *proc;
    
    DisablePreemption();
    
    while ((proc = LIST_HEAD(&rendez->process_list)) != NULL)
    {
        LIST_REM_HEAD (&rendez->process_list, rendez_link);
        proc->sleeping_on = NULL;
        proc->state = PROC_STATE_READY;
        SchedReady(proc);
    }
}




/*
 * Wakes up a single specified process that is sleeping on a rendez.
 * Removes said process from the rendez list of sleeping processes.
 */

void WakeupProcess (struct Process *proc)
{
    struct Rendez *rendez;
    
    DisablePreemption();
    
    KASSERT (proc->state == PROC_STATE_SLEEP);
    
    rendez = proc->sleeping_on;
    LIST_REM_ENTRY (&rendez->process_list, proc, rendez_link);
    proc->sleeping_on = NULL;
    proc->state = PROC_STATE_READY;
    SchedReady(proc);
}






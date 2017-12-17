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
 * Process creation system calls
 */

#include <kernel/types.h>
#include <kernel/proc.h>
#include <kernel/dbg.h>
#include <kernel/utility.h>
#include <kernel/error.h>
#include <kernel/globals.h>
#include <kernel/vm.h>



/*
 * Create a new process, Only Executive.has permission to create a new process.
 * Takes a table of pointers to segments and a table of handles to pass to the
 * child process.
 *
 * If removing page table entries in PmapRemove operations are atomic we can
 * move it before disabling preemption.
 *

 */

SYSCALL int Fork (bits32_t flags)
{
    struct Process *proc, *current;
    int h;
    
    
    current = GetCurrentProcess();
    
    DisablePreemption();
    
    if ((h = AllocHandle()) == -1)
        return resourceErr;
        
    if ((proc = AllocProcess()) == NULL)
    {
        FreeHandle (h);
        return resourceErr;
    }

    proc->handle = h;
    proc->flags = 0;
    
    if (ArchForkProcess(proc, current) != 0)
    {
        FreeProcess (proc);
        FreeHandle (h);
        return resourceErr;
    } 
    
    if (ForkAddressSpace (&proc->as, &current->as) != 0)
    {
        FreeProcess(proc);
        FreeHandle(h);
        return resourceErr;
    }
    
    SetObject (current, h, HANDLE_TYPE_PROCESS, proc);    
        
    proc->state = PROC_STATE_READY;
    SchedReady(proc);

    return h;
}




















/*
 * AllocProcess();
 */
 
struct Process *AllocProcess (void)
{
    struct Process *proc;
    
    
    proc = LIST_HEAD (&free_process_list);
        
    KASSERT (proc != NULL);
    
    LIST_REM_HEAD (&free_process_list, free_entry);
    
    MemSet (proc, 0, sizeof *proc);

    proc->state = PROC_STATE_INIT;
    proc->exit_status = 0;
    proc->quanta_used = 0;
    proc->sched_policy = SCHED_OTHER;
    proc->tickets = STRIDE_DEFAULT_TICKETS;
    proc->stride = STRIDE1 / proc->tickets;
    proc->remaining = 0;
    proc->pass = global_pass;
    proc->continuation_function = NULL;
//    proc->virtualalloc_sz = 0;
    
    InitRendez(&proc->waitfor_rendez);
    
    LIST_INIT (&proc->pending_handle_list);
    LIST_INIT (&proc->close_handle_list);
    
    free_process_cnt --;
    
    return proc;
}       




/*
 * FreeProcess();
 *
 * Only parent can free process struct.
 */

void FreeProcess (struct Process *proc)
{   
    LIST_ADD_HEAD (&free_process_list, proc, free_entry);
    proc->state = PROC_STATE_UNALLOC;

    free_process_cnt ++;
}



struct Process *GetProcess (int idx)
{
    KASSERT (idx >= 0 && idx < max_process);
    
    return (struct Process *)((vm_addr)process_table + idx * PROCESS_SZ);
}

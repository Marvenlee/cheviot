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
 */

SYSCALL int Spawn (vm_addr *user_iov, int niov, void *entry, void *stack_top,
    int *u_sysports, bits32_t flags)
{
    struct Process *proc, *current;
    
    struct MemArea *matable[MAX_IOV_SEGMENTS];
    int sysports[NSYSPORT];
    int h;  
    int t;
    
    
    current = GetCurrentProcess();

    if (!(current->flags & PROCF_EXECUTIVE))
        return privilegeErr;
        
    CopyIn (&sysports, u_sysports, sizeof sysports);
    
    if (ValidateSystemPorts (sysports) < 0)
        return paramErr;
    
    if (CopyInIOVs (matable, user_iov, niov) < 0)
        return paramErr;

    if (free_handle_cnt < 1 || free_process_cnt < 1)
        return resourceErr;
    
    DisablePreemption();
    
    h = AllocHandle();
    proc = AllocProcess();
    proc->handle = h;
    proc->flags = flags;
    SetObject (current, h, HANDLE_TYPE_PROCESS, proc);
    ArchAllocProcess(proc, entry, stack_top);
    TransferSystemPorts (proc, sysports);
    
    for (t=0; t < niov; t++)
    {
        PmapRemoveRegion (&current->pmap, matable[t]);
        matable[t]->owner_process = proc;
    }


    proc->state = PROC_STATE_READY;
    SchedReady(proc);
    return h;
}





/*
 * Returns information about the system handles (and channels) that were
 * passed to the process by Spawn().
 */

SYSCALL int GetSystemPorts (int *sysports)
{
    struct Process *current;
    
    current = GetCurrentProcess();
    CopyOut (sysports, current->system_port, sizeof (int) * 8);
    return 0;
}




/*
 * Checks the handles passed by Spawn() belong to the process (Executive)
 * calling Spawn().  Makes sure there are no duplicates.
 */
 
int ValidateSystemPorts (int *sysports)
{
    int s, t;
    struct Process *current;
    
    current = GetCurrentProcess();
    
    for (t=0; t< NSYSPORT; t++)
    {
        if (sysports[t] != -1)
        {
            for (s=0; s<t; s++)
            {
                if (sysports[t] == sysports[s])
                    return paramErr;
            }

            if (GetHandle (current, sysports[t]) == NULL)
                return paramErr;
        }
    }
    
    return 0;
}




/*
 * Transfers ownership of the handles passed by Spawn() to the
 * child process.  Marks the system handles as GRANT_ONCE so 
 * they cannot be passed to other processes
 */

void TransferSystemPorts (struct Process *proc, int *sysports)
{
    int t;
    
    for (t=0; t< NSYSPORT; t++)
    {    
        proc->system_port[t] = sysports[t]; 
        
        if (sysports[t] != -1)
        {
            handle_table[sysports[t]].owner = proc;
            handle_table[sysports[t]].flags |= HANDLEF_GRANT_ONCE;
        }
    }
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





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
 * Function to return stats of kernel resources.
 */

#include <kernel/types.h>
#include <kernel/proc.h>
#include <kernel/vm.h>
#include <kernel/timer.h>
#include <kernel/sysinfo.h>
#include <kernel/utility.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>





/*
 * Return general information about the system
 *
 * FIXME: finish SystemInfo(),  count total pages of processes
 * waiting for memory
 */

SYSCALL int SystemInfo (sysinfo_t *si_user)
{
    sysinfo_t si;
    struct Process *current;
    
    current = GetCurrentProcess();
    
    MemSet (&si, 0, sizeof si);
    si.flags = 0;
    si.cpu_cnt = max_cpu;
    si.process_cnt = max_process;
    si.total_pages = 0;       // fixme : count pages in initvm

  

    /*
    for (t=0; t<max_process; t++)
    {
        if (proc = GetProcess(t) != NULL)
        {
            if (proc->state != PROC_STATE_FREE)
            {
                si.virtualalloc_requested = 0;
            }
        }
    }
    */
    
    si.page_size = PAGE_SIZE;
    si.virtualalloc_alignment = PAGE_SIZE;
    si.max_memareas = max_vseg;
    si.max_handles = max_handle;
    si.max_tickets = STRIDE_MAX_TICKETS;

    if (current->flags & PROCF_DAEMON)
        MemCpy (si_user, &si, sizeof *si_user);
    else
        CopyOut (si_user, &si, sizeof *si_user);
    
    return 0;
}






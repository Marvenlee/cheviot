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
#include <kernel/utility.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>
#include <limits.h>




/*
 * Returns information about the system handles (and channels) that were
 * passed to the process by Spawn().
 */

SYSCALL int ProcessInfo (struct ProcessInfo *pi_result)
{
    struct Process *current;
    struct ProcessInfo pi;
    
    current = GetCurrentProcess();
    
    pi.sighangup_handle     = current->sighangup_handle;
    pi.sigterm_handle       = current->sigterm_handle;
    pi.siglowmem_handle     = current->siglowmem_handle;
    pi.namespace_handle     = current->namespace_handle;

    pi.argv = current->argv;
    pi.argc = current->argc;
    pi.envv = current->envv;
    pi.envc = current->envc;
            
    CopyOut (pi_result, &pi, sizeof pi);
    return 0;
}






/*
 * Return general information about the system
 *
 * FIXME: finish SystemInfo(),  count total pages of processes
 * waiting for memory
 */

SYSCALL int SystemInfo (sysinfo_t *si_user)
{
    sysinfo_t si;

/*
    struct Process *current;
    struct Process *proc;
    int t;
    size_t virtualalloc_total_sz;
    size_t virtualalloc_min_sz;
    size_t virtualalloc_max_sz;
    int virtualalloc_process_cnt;
    
    
    virtualalloc_min_sz = 0;
    virtualalloc_max_sz = 0;
    virtualalloc_process_cnt = 0;
    virtualalloc_total_sz = 0;
    
    current = GetCurrentProcess();
    
    MemSet (&si, 0, sizeof si);

    si.flags = 0;
    si.cpu_cnt = cpu_cnt;
    si.max_process = max_process;
    si.free_process_cnt = free_process_cnt;
        
    si.avail_mem_sz = avail_mem_sz;
    si.total_mem_sz = total_mem_sz;

    si.vseg_cnt = vseg_cnt;
    si.max_vseg = max_vseg;
    si.pseg_cnt = pseg_cnt;
    si.max_pseg = max_pseg;
    
    si.page_size = PAGE_SIZE;
    si.mem_alignment = PAGE_SIZE;
    
    si.free_handle_cnt = free_handle_cnt;
    si.max_handle = max_handle;
    si.max_ticket = STRIDE_MAX_TICKETS;
    si.default_ticket = STRIDE_DEFAULT_TICKETS;
 */   
    CopyOut (si_user, &si, sizeof *si_user);
    
    return 0;
}






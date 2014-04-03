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
 *
 *
 */

SYSCALL int SystemInfo (sysinfo_t *si_user)
{
    sysinfo_t si;
    
    MemSet (&si, 0, sizeof si);
    si.flags = 0;
    si.cpu_cnt = max_cpu;
    si.process_cnt = max_process;
    si.total_pages = max_segment;
    si.page_size = PAGE_SIZE;
    si.virtualalloc_alignment = PAGE_SIZE;
    si.max_memareas = max_memarea;
    si.max_handles = max_handle;
    si.max_tickets = STRIDE_MAX_TICKETS;

    CopyOut (si_user, &si, sizeof (sysinfo_t));
    return 0;
}






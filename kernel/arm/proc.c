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
 * ARM-specific process creation and deletion code.
 */
 
#include <kernel/types.h>
#include <kernel/proc.h>
#include <kernel/utility.h>
#include <kernel/dbg.h>
#include <kernel/arm/init.h>
#include <kernel/globals.h>



/*
 * ArchAllocProcess();
 * Executed on the context of the parent process. Initializes CPU state
 * and kernel stack so that PrepareProcess() is executed on the context
 * of the new process.
 */

int ArchForkProcess (struct Process *proc, struct Process *current)
{
    int sc;
    
    KLog ("ArchForkProcess()");
    
    proc->task_state.cpu = &cpu_table[0];
    proc->task_state.flags = 0;

    if ((sc = PmapInit (&proc->as)) != 0)
    {
        return sc;
    }

    MemCpy (proc, current, sizeof *current);
    proc->task_state.r0 = proc->handle;
    return 0;
}






/*
 *
 */

void ArchFreeProcess (struct Process *proc)
{
}





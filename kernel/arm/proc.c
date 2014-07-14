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

void ArchAllocProcess (struct Process *proc, void *entry, void *stack)
{
    KLog ("ArchInitProc()");
    
    proc->task_state.cpu = &cpu_table[0];
    proc->task_state.flags = 0;

    PmapInit (proc);

    proc->task_state.pc = (uint32)entry;
    proc->task_state.r0 = 0;
    proc->task_state.cpsr = cpsr_dnm_state | USR_MODE | CPSR_DEFAULT_BITS;
    proc->task_state.r1 = 0;
    proc->task_state.r2 = 0;
    proc->task_state.r3 = 0;
    proc->task_state.r4 = 0;
    proc->task_state.r5 = 0;
    proc->task_state.r6 = 0;
    proc->task_state.r7 = 0;
    proc->task_state.r8 = 0;
    proc->task_state.r9 = 0;
    proc->task_state.r10 = 0;
    proc->task_state.r11 = 0;
    proc->task_state.r12 = 0;
    proc->task_state.sp = (uint32)stack;
    proc->task_state.lr = 0;
}




/*
 *
 */

void ArchFreeProcess (struct Process *proc)
{
}





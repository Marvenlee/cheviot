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
 * KernelExit function.  Called prior to returning to user-mode by
 * Software Interrupts (SWI), interrupts and exceptions.  Also the
 * path that is taken whenever a process is put to sleep in the middle
 * of a system call.
 */

#include <kernel/types.h>
#include <kernel/error.h>
#include <kernel/proc.h>
#include <kernel/arm/arm.h>
#include <kernel/arm/task.h>
#include <kernel/arm/globals.h>
#include <kernel/dbg.h>




/*
 * __KernelExit is the exit pathway out of the kernel for system calls,
 * interrupts and exceptions.  They all call __KernelExit which in turn
 * calls KernelExit.
 *
 * This performs deferred procedure calls of timer and interrupt bottom half
 * processing as well as exit detection.
 *
 */

void KernelExit (void)
{
    struct CPU *cpu;
    struct Process *current;

    cpu = GetCPU();

    DisablePreemption();
    TimerBottomHalf();
    
    DisableInterrupts();
    InterruptBottomHalf();
            
    if (cpu->reschedule_request == TRUE)
    {
        Reschedule();
        cpu->reschedule_request = FALSE;
        EnableInterrupts();
        __KernelExit();
    }

    EnableInterrupts();
    EnablePreemption();
    
    current = GetCurrentProcess();

    if (current->task_state.flags != 0)
    {
        if (current->task_state.flags & TSF_EXIT)
            Exit (current->exit_status);
        else if (current->task_state.flags & TSF_KILL)
            Exit(EXIT_KILLED);
        else if (current->task_state.flags & TSF_EXCEPTION)
            Exit(EXIT_FATAL);
    }
    
    ClosePendingHandles();
    
    EnablePreemption();
    
    while (current->continuation_function != NULL)
        current->continuation_function();
    
}









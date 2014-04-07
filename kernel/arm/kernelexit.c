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
 * Notes:
 *
 * Unsure how SMP will affect the inkernel_now, inkernel_lock and interrupt
 * handling.
 *
 * Any continuation function needs to check TSF flags
 * in case CopyIn() or CopyOut() causes an exception and so avoid
 * an infinite loop.
 *
 * Continuation is useful for VirtualAlloc.  Memory can be mapped in supervisor
 * mode, wiped clean with preemption enabled in a continuation and then
 * finally set to user-mode protection.
 *
 * taskstate.flags may become Unix-like signals, such as SIGKILL, SIGTERM,
 * SIGHUP etc.  These could be delivered to user-mode from within this function.
 * However, Unix advances the program counter when a system call is interrupted.
 * The PC-lusering in this microkernel doesn't advance the program counter, so
 * the system call restarts.
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

    if (current->continuation_function != NULL)
    {
        // May want to set task_state->r0
        // May want to advance program counter (if syscall function didn't exit
        // normally.
        
        current->continuation_function();
        EnablePreemption();
    }
        
    if (current->task_state.flags != 0)
    {
        if (current->task_state.flags & TSF_EXIT)
            DoExit (current->exit_status);
        else if (current->task_state.flags & TSF_KILL)
            DoExit(EXIT_KILLED);
        else if (current->task_state.flags & TSF_EXCEPTION)
            DoExit(EXIT_FATAL);
    }
    
    ClosePendingHandles();
}









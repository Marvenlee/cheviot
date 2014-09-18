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

#include <kernel/types.h>
#include <kernel/proc.h>
#include <kernel/error.h>
#include <kernel/dbg.h>
#include <kernel/timer.h>
#include <kernel/arm/arm.h>
#include <kernel/utility.h>
#include <kernel/vm.h>
#include <kernel/arm/globals.h>




/*
 *
 */
 
void ResetHandler (void)
{
    KernelPanic ("Reset Handler ");
}




/*
 *
 */

void FiqHandler (void)
{
    KernelPanic ("FIQ Interrupt Handler ");
}




/*
 *
 */

void UnknownSyscall (void)
{
    struct Process *current;
    
    KLog ("Unknown Syscall");
    
    current = GetCurrentProcess();
        
    current->task_state.exception = EI_UNDEFSYSCALL;
    current->task_state.fault_addr = current->task_state.pc;
    current->task_state.fault_access = 0;
    current->task_state.flags |= TSF_EXCEPTION;
}




/*
 *
 */

void UndefInstrHandler (void)
{
    struct Process *current;
    bits32_t spsr;

    spsr = GetSPSR();
    current = GetCurrentProcess();
    
    
    KLog ("Undefined Instruction");
    KLog ("pc = %#010x, sp = %#010x", current->task_state.pc, current->task_state.sp);
    
    
    if (!((spsr & CPSR_MODE_MASK) == USR_MODE
        || (spsr & CPSR_MODE_MASK) == SYS_MODE))
    {
        KernelPanic ("Undefined instruction in kernel");
    }
    
    current->task_state.exception = EI_UNDEFINSTR;
    current->task_state.fault_addr = current->task_state.pc;
    current->task_state.fault_access = 0;
    current->task_state.flags |= TSF_EXCEPTION;


    // FIXME: Return straight to __KernelExit();

    __KernelExit();
}




/*
 *
 */

void PrefetchAbortHandler (void)
{
    struct Process *current;
    bits32_t spsr;

    current = GetCurrentProcess();  
        
    KLog ("Prefetch Abort Handler");
    KLog ("pc = %#010x, sp = %#010x", current->task_state.pc, current->task_state.sp);
        
    spsr = GetSPSR();
        
    if (!((spsr & CPSR_MODE_MASK) == USR_MODE
        || (spsr & CPSR_MODE_MASK) == SYS_MODE))
    {
        KernelPanic ("Instruction prefetch fault in kernel");
    }
        
    
    current->task_state.fault_addr = current->task_state.pc;
    current->task_state.fault_access = PROT_EXEC;
    current->task_state.flags |= TSF_PAGEFAULT;
    PmapPageFault();
}




/*
 *
 */

void DataAbortHandler (void)
{
    struct Process *current;
    bits32_t dfsr;
    bits32_t access;
    
    current = GetCurrentProcess();
    KLog ("Data Abort Handler");
    KLog ("pc = %#010x, sp = %#010x", current->task_state.pc, current->task_state.sp);

    if (inkernel_lock == 1)
        KernelPanic ("Kernel page fault with inkernel_lock held");
    
    dfsr = GetDFSR();

    if (dfsr & DFSR_RW)
        access = PROT_WRITE;
    else
        access = PROT_READ;
    
    current->task_state.fault_addr = GetFAR();
    current->task_state.fault_access = access;
    current->task_state.flags |= TSF_PAGEFAULT;
    PmapPageFault();
}




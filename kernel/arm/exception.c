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
    DoPageFault();
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
    DoPageFault();
}




/*
 *
 */

void DoPageFault (void)
{
    struct Process *current;
    struct MemArea *ma;
    vm_addr addr;
    bits32_t access;
    struct Segment *seg;
    
//    KLog ("DoPageFault()");
    
    current = GetCurrentProcess();
        
    KASSERT (current != NULL);
    
    addr = current->task_state.fault_addr;
    access = current->task_state.fault_access;
    addr = ALIGN_DOWN (addr, PAGE_SIZE);
    ma = MemAreaFind (addr);

/*  
    if (access & PROT_READ)
    {
        KLog ("addr = %#010x,  PROT_READ, ma = %#010x", addr, ma);
    }
    else if (access & PROT_WRITE)
    {
        KLog ("addr = %#010x,  PROT_WRITE, ma = %#010x", addr, ma);
    }
    else if (access & PROT_EXEC)
    {
        KLog ("addr = %#010x,  PROT_EXEC, ma = %#010x", addr, ma);
    }
    else
    {
        KLog ("addr = %#010x,  PROT UNKNOWN, ma = %#010x", addr, ma);
    }
    
    KASSERT (addr >= user_base && addr < user_ceiling);
    KASSERT (ma != NULL);
    KASSERT (ma->owner_process == current);
    KASSERT (MEM_TYPE(ma->flags) != MEM_FREE);
    KASSERT (MEM_TYPE(ma->flags) != MEM_RESERVED);
    KASSERT ((access & ma->flags) == access);
*/
    
    if (addr < user_base || addr >= user_ceiling
            || ma == NULL
            || ma->owner_process != current
            || MEM_TYPE(ma->flags) == MEM_FREE
            || MEM_TYPE(ma->flags) == MEM_RESERVED
            || (access & ma->flags) != access)
    {
        current->task_state.flags |= TSF_EXCEPTION;
        DoExit(EXIT_FATAL);
    }


    DisablePreemption();


    if (MEM_TYPE(ma->flags) == MEM_ALLOC)
    {
        seg = SegmentFind (ma->physical_addr);
        
        if (seg->busy == TRUE)
            Sleep (&vm_rendez);
    }

    if (ma->flags & MAP_VERSION && access & PROT_WRITE)
    {
        ma->flags &= ~MAP_VERSION;
        ma->version = memarea_version_counter;
        memarea_version_counter ++;
    }
    
    PmapEnterRegion(&current->pmap, ma, addr);

    
    current->task_state.flags &= ~TSF_PAGEFAULT;
    KLog ("Returning from fault");
}














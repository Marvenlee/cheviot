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
 * Kernel initialization.
 */

#include <kernel/types.h>
#include <kernel/vm.h>
#include <kernel/proc.h>
#include <kernel/dbg.h>
#include <kernel/utility.h>
#include <kernel/arm/init.h>
#include <kernel/arch.h>
#include <kernel/globals.h>
#include <kernel/arm/boot.h>


// Variables
extern int svc_stack_top;
extern int interrupt_stack_top;
extern int exception_stack_top;
extern int idle_stack_top;

#define PROCF_USER            0         // User process
#define PROCF_KERNEL          (1 << 0)  // Kernel task: For kernel daemons

struct Process *CreateProcess (void (*entry)(void), void *stack, void (*continuation)(void),
    int policy, int priority, bits32_t flags, struct CPU *cpu);

/*
 * InitProc();
 *
 * Initializes kernel tables relating to processes, ipc, scheduling and timers.
 * Starts the first process, root_process by calling SwitchToRoot().
 * This loads the FPU state, switches the stack pointer into the kernel
 * and reloads the page directory.  When this is done no lower memory which
 * includes this initialization code will be visible.
 */

void InitProcesses (void)
{
    uint32 t;
    struct Process *proc;
    struct Timer *timer;
    struct ISRHandler *isr_handler;
    struct Handle *handle;
    struct CPU *cpu;

    
    KLog ("InitProcesses()");
        
        
    for (t=0;t<NIRQ;t++)
    {
        LIST_INIT (&isr_handler_list[t]);
    }
        
    for (t=0; t<32; t++)
    {
        CIRCLEQ_INIT (&realtime_queue[t]);
    }
    
    LIST_INIT (&stride_queue);
    
    
    LIST_INIT (&free_process_list)
    free_process_cnt = max_process;
    
    KLog ("InitProcesses() - isr, realtime, stride and free process done");

        
    for (t=0; t<max_process; t++)
    {
        proc = (struct Process *)((uint8 *)process_table + t*PROCESS_SZ);
        
        LIST_ADD_TAIL (&free_process_list, proc, free_entry);
        proc->state = PROC_STATE_UNALLOC;
    }

    KLog ("InitProcesses() - processes added to free list");
    
    
    // Move to inittimer ?
    
    for (t=0; t<JIFFIES_PER_SECOND; t++)
    {
        LIST_INIT(&timing_wheel[t]);
    }
        
    softclock_seconds = hardclock_seconds = 0;
    softclock_jiffies = hardclock_jiffies = 0;

    LIST_INIT (&free_timer_list);
    free_timer_cnt = max_timer;
    
    for (t=0; t < max_timer; t++)
    {
        timer = &timer_table[t];
        LIST_ADD_TAIL (&free_timer_list, timer, timer_entry);
    }

    KLog ("InitProcesses() - timer lists done");

    
    
    LIST_INIT (&free_isr_handler_list);
    free_isr_handler_cnt = max_isr_handler;
    
    for (t=0; t < max_isr_handler; t++)
    {
        isr_handler = &isr_handler_table[t];
        LIST_ADD_TAIL (&free_isr_handler_list, isr_handler, isr_handler_entry);
    }
    
    KLog ("InitProcesses() - free isr list done");

    
    LIST_INIT (&free_handle_list);
    free_handle_cnt = max_handle;
    
    for (t = 0; t < max_handle; t++)
    {
        handle = &handle_table[t];
        handle->type = HANDLE_TYPE_FREE;
        handle->pending = 0;
        handle->owner = NULL;
        handle->object = NULL;
        handle->flags = 0;
        
        LIST_ADD_TAIL (&free_handle_list, handle, link);
    }
      
    KLog ("InitProcesses() - free handle list done");
   
    KLog("Init root process");
    
    max_cpu = 1;
    cpu_cnt = max_cpu;
    cpu = &cpu_table[0];
    

    root_process = CreateProcess (bootinfo->root_entry_point, (void *)bootinfo->root_stack_top,
                                    NULL, SCHED_RR, 1, PROCF_USER, &cpu_table[0]);
        
    root_process->state = PROC_STATE_RUNNING;
    cpu->current_process = root_process;
    cpu->reschedule_request = 0;
    cpu->svc_stack = (vm_addr)&svc_stack_top;
    cpu->interrupt_stack = (vm_addr)&interrupt_stack_top;
    cpu->exception_stack = (vm_addr)&exception_stack_top;

    idle_task = CreateProcess (IdleTask, (void *)idle_stack_top,
                                    NULL,  SCHED_RR, 0, PROCF_KERNEL, &cpu_table[0]);
                                    
    KLog ("InitProcesses() DONE");
}


/*!
    Hmmm, can't we simplify this for root and idle processes?
*/     
struct Process *CreateProcess (void (*entry)(void), void *stack, void (*continuation)(void),
    int policy, int priority, bits32_t flags, struct CPU *cpu)
{    
    int handle;
    struct Process *proc;

    KLog ("CreateProcess()");  
    
    if ((proc = AllocProcess()) == NULL)
    {
        return NULL;
    }
    
    if ((handle = AllocHandle()) == -1)
    {
        return NULL;
    }
        
    SetObject (proc, handle, HANDLE_TYPE_PROCESS, proc);
    
    proc->continuation_function = continuation;        
    proc->handle = handle;
    proc->exit_status = 0;
    proc->virtualalloc_size = 0;
    proc->flags = flags;
    
    InitRendez(&proc->waitfor_rendez);
        
    LIST_INIT (&proc->pending_handle_list);
    LIST_INIT (&proc->close_handle_list);
    
    proc->as.pmap->l1_table = (void *)VirtToPhys(bootinfo->root_pagedir);
    
    proc->task_state.cpu = cpu;
    proc->task_state.flags = 0;
    proc->task_state.pc = (uint32)entry;
    proc->task_state.r0 = 0;
    
    if (proc->flags & PROCF_KERNEL) {
        proc->task_state.cpsr = cpsr_dnm_state | SYS_MODE | CPSR_DEFAULT_BITS;
    }
    else {
        proc->task_state.cpsr = cpsr_dnm_state | USR_MODE | CPSR_DEFAULT_BITS;
    }
    
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

    KLog ("Proc add to run queue");

    if (policy == SCHED_RR || policy == SCHED_FIFO)
    {
        proc->quanta_used = 0;
        proc->sched_policy = policy;
        proc->tickets = priority;
        SchedReady (proc);
    }
    else if (policy == SCHED_OTHER)
    {
        proc->quanta_used = 0;
        proc->sched_policy = policy;
        proc->tickets = priority;
        proc->stride = STRIDE1 / proc->tickets;
        proc->remaining = proc->stride;
        proc->pass = global_pass;
        SchedReady (proc);
    }
    else if (policy == SCHED_IDLE)
    {
        cpu->idle_process = proc;
    }

    KLog ("CreateProcess() DONE");

    return proc;
}



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
 * Process and non-VM kernel resource initialization.
 */

#include <kernel/types.h>
#include <kernel/vm.h>
#include <kernel/proc.h>
#include <kernel/dbg.h>
#include <kernel/utility.h>
#include <kernel/arch.h>
#include <kernel/arm/init.h>
#include <kernel/globals.h>
#include <kernel/arm/boot.h>


extern int svc_stack_top;
extern int interrupt_stack_top;
extern int exception_stack_top;

//extern int vm_task_stack_top;
//extern int idle_task_stack_top;


void InitProcesses(void);
void IdleTask (void);
void ReaperTask (void);
void VMTask (void);



struct Process *CreateProcess (void (*entry)(void), void *stack,
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

void InitProc (void)
{
    KLOG ("InitProc() ***");
    
    InitProcessTables();
    InitProcesses();
}







/*
 *
 */

void InitProcessTables (void)
{
    uint32 t;
    struct Process *proc;
    struct Timer *timer;
    struct ISRHandler *isr_handler;
    struct Channel *channel;
    struct Handle *handle;
    struct Parcel *parcel;
    
    KPRINTF ("InitProcessTables()");
        
        
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
        
    for (t=0; t<max_process; t++)
    {
        proc = (struct Process *)((uint8 *)process_table + t*PROCESS_SZ);
        
        LIST_ADD_TAIL (&free_process_list, proc, free_entry);
        proc->state = PROC_STATE_UNALLOC;
    }
    
    for (t=0; t<JIFFIES_PER_SECOND; t++)
    {
        LIST_INIT(&timing_wheel[t]);
    }
        
    softclock_seconds = hardclock_seconds = 0;
    softclock_jiffies = hardclock_jiffies = 0;

    LIST_INIT (&free_timer_list);
    
    for (t=0; t < max_timer; t++)
    {
        timer = &timer_table[t];
        LIST_ADD_TAIL (&free_timer_list, timer, timer_entry);
    }
        
    LIST_INIT (&free_isr_handler_list);
    
    for (t=0; t < max_isr_handler; t++)
    {
        isr_handler = &isr_handler_table[t];
        LIST_ADD_TAIL (&free_isr_handler_list, isr_handler, isr_handler_entry);
    }
        
        
    LIST_INIT (&free_channel_list);
    
    for (t=0; t < max_channel; t++)
    {
        channel = &channel_table[t];
        LIST_ADD_TAIL (&free_channel_list, channel, link);
    }
    
    
    LIST_INIT (&free_handle_list);
    
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
    
    
    LIST_INIT (&free_parcel_list);
    
    for (t=0; t < max_parcel; t++)
    {
        parcel = &parcel_table[t];
        LIST_ADD_TAIL (&free_parcel_list, parcel, link);
    }
}




/*
 * Initializes root user process followed by idle and vm_task kernel processes.
 */

void InitProcesses (void)
{   
    struct CPU *cpu;
    int t;
    
   
    KLog("InitProcesses()");
    
    max_cpu = 1;
    cpu_cnt = max_cpu;
    cpu = &cpu_table[0];
    
    
    
    idle_task = CreateProcess (IdleTask, &idle_task_stack_top, SCHED_IDLE, 0, PROCF_DAEMON, &cpu_table[0]);
    vm_task = CreateProcess (VMTask, &vm_task_stack_top, SCHED_OTHER, 0, PROCF_DAEMON, &cpu_table[0]);

    root_process = CreateProcess (bootinfo->procman_entry_point, (void *)bootinfo->user_stack_ceiling,
                                    SCHED_OTHER, 100, PROCF_FILESYS, &cpu_table[0]);
                    
    for (t=0; t < seg_cnt; t++)
    {
        if ((seg_table[t].flags & MEM_MASK) == MEM_ALLOC)
        {
            seg_table[t].owner = root_process;
            seg_table[t].segment_id = next_unique_segment_id ++;
        }
    }    
    
    root_process->state = PROC_STATE_RUNNING;
    cpu->current_process = root_process;
    cpu->reschedule_request = 0;
    cpu->svc_stack = (vm_addr)&svc_stack_top;
    cpu->interrupt_stack = (vm_addr)&interrupt_stack_top;
    cpu->exception_stack = (vm_addr)&exception_stack_top;
}





/*
 *
 */
     
struct Process *CreateProcess (void (*entry)(void), void *stack, int policy, int priority, bits32_t flags, struct CPU *cpu)
{    
    int handle;
    struct Process *proc;
  
    
    proc = AllocProcess();
    handle = AllocHandle();

    KASSERT (handle >= 0);
        
    SetObject (proc, handle, HANDLE_TYPE_PROCESS, proc);
    
    proc->handle = handle;
    proc->exit_status = 0;
    
    proc->flags = flags;
    
   // FIXME: Move into AllocProcess ????????????????
    LIST_INIT (&proc->pending_handle_list);
    LIST_INIT (&proc->close_handle_list);
    
        
    PmapInit (proc);
    
    proc->task_state.cpu = cpu;
    proc->task_state.flags = 0;
    proc->task_state.pc = (uint32)entry;
    proc->task_state.r0 = 0;
    
    if (proc->flags & PROCF_DAEMON) {
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


    return proc;
}
























    


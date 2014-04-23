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


void InitProcesses(void);
void IdleTask (void);
void VMTask (void);
void ReaperTask (void);

struct Process *InitRootProcess (void (*entry)(void), void *stack,
    int policy, int priority);
struct Process *InitDaemon (void (*entry)(void), void *stack,
    int policy, int priority);
void InitSchedParams (struct Process *proc, int policy, int priority);



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
    struct Notification *notif;
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
    
    LIST_INIT (&free_notification_list);
    
    for (t=0; t < max_notification; t++)
    {
        notif = &notification_table[t];
        LIST_ADD_TAIL (&free_notification_list, notif, link);
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
 * InitRoot();
 * FIXME: Why can't we call AllocProcess() ?
 * Reason: AllocProcess() depends on GetCurrentProcess() to inherit
 * parent values.
 */

void InitProcesses (void)
{   
    struct CPU *cpu;
   
   
    KLog("InitProcesses()");
    
    max_cpu = 1;
    cpu = &cpu_table[0];

    root_process = InitRootProcess (bootinfo->procman_entry_point,
                    (void *)bootinfo->user_stack_ceiling, SCHED_OTHER, 100);
                    
    idle_task    = InitDaemon (IdleTask, (void *)&idle_task_stack_top, SCHED_IDLE, 0);
    vm_task      = InitDaemon (VMTask, (void *)&vm_task_stack_top, SCHED_OTHER, 100);
    reaper_task  = InitDaemon (ReaperTask, (void *)&reaper_task_stack_top, SCHED_OTHER, 100);
    
    root_process->state = PROC_STATE_RUNNING;
    cpu->current_process = root_process;
    cpu->idle_process = idle_task;
    cpu->reschedule_request = 0;
    cpu->svc_stack = (vm_addr)&svc_stack_top;
    cpu->interrupt_stack = (vm_addr)&interrupt_stack_top;
    cpu->exception_stack = (vm_addr)&exception_stack_top;
}





/*
 *
 */
     
struct Process *InitRootProcess (void (*entry)(void), void *stack,
    int policy, int priority)
{    
    int handle;
    struct Process *proc;
    int t;
    
    
    proc = AllocProcess();
    handle = AllocHandle();

    KASSERT (handle >= 0);
        
    SetObject (proc, handle, HANDLE_TYPE_PROCESS, proc);
    
    proc->handle = handle;
    proc->exit_status = 0;
    proc->virtualalloc_sz = 0;
    
    LIST_INIT (&proc->pending_handle_list);
    LIST_INIT (&proc->close_handle_list);
    
    PmapInit (&proc->pmap);
    
    proc->task_state.cpu = &cpu_table[0];
    proc->task_state.flags = 0;
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

    for (t=0; t<vseg_cnt; t++)
    {
        if ((vseg_table[t].flags & MEM_MASK) == MEM_ALLOC)
        {
            vseg_table[t].owner = proc;
            vseg_table[t].version = segment_version_counter;
            segment_version_counter++;
        }
    }
    
    InitSchedParams (proc, policy, priority);
    return proc;
}








struct Process *InitDaemon (void (*entry)(void), void *stack,
    int policy, int priority)
{
    struct Process *proc;
    
    proc = AllocProcess();
    
    proc->exit_status = 0;
    proc->virtualalloc_sz = 0;

    LIST_INIT (&proc->pending_handle_list);
    LIST_INIT (&proc->close_handle_list);

    proc->task_state.cpu = &cpu_table[0];
    proc->task_state.flags = 0;
    proc->task_state.pc = (uint32)entry;
    proc->task_state.r0 = 0;
    proc->task_state.cpsr = cpsr_dnm_state | SYS_MODE | CPSR_DEFAULT_BITS;
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

    InitSchedParams (proc, policy, priority);
    return proc;
}




/*
 *
 */
     
void InitSchedParams (struct Process *proc, int policy, int tickets)
{
    if (policy == SCHED_RR || policy == SCHED_FIFO)
    {
        proc->quanta_used = 0;
        proc->sched_policy = policy;
        proc->tickets = tickets;
        SchedReady (proc);
    }
    else if (policy == SCHED_OTHER)
    {
        proc->quanta_used = 0;
        proc->sched_policy = policy;
        proc->tickets = tickets;
        proc->stride = STRIDE1 / proc->tickets;
        proc->remaining = proc->stride;
        proc->pass = global_pass;
        SchedReady (proc);
    }
    else if (policy == SCHED_IDLE)
    {

    }
}























    


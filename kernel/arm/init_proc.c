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
void InitRootAddressSpace(void);
void idle_loop (void);


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
    InitRootAddressSpace();
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
    int handle;
    
    KLog("InitProcesses()");
    
    max_cpu = 1;
    cpu = &cpu_table[0];

    root_process = LIST_HEAD (&free_process_list);
    
    KASSERT (root_process != NULL);
    
    LIST_REM_HEAD (&free_process_list, free_entry);

    handle = AllocHandle();

    KASSERT (handle >= 0);
        
    SetObject (root_process, handle, HANDLE_TYPE_PROCESS, root_process);
    
    root_process->handle = handle;
    root_process->exit_status = 0;
    root_process->sched_policy = SCHED_RR;
    root_process->quanta_used = 0;      
    root_process->tickets = 16;
    root_process->stride = 0;
    root_process->remaining = 0;
    root_process->pass = 0;
    root_process->continuation_function = NULL;
    root_process->virtualalloc_sz = 0;
    
    LIST_INIT (&root_process->pending_handle_list);
    LIST_INIT (&root_process->close_handle_list);
    
    
    PmapInit (&root_process->pmap);
    
    root_process->task_state.cpu = &cpu_table[0];
    root_process->task_state.flags = 0;
    
    root_process->task_state.pc = (uint32)bootinfo->procman_entry_point;
    root_process->task_state.r0 = 0;
    root_process->task_state.cpsr = cpsr_dnm_state | USR_MODE | CPSR_DEFAULT_BITS;
    root_process->task_state.r1 = 0;
    root_process->task_state.r2 = 0;
    root_process->task_state.r3 = 0;
    root_process->task_state.r4 = 0;
    root_process->task_state.r5 = 0;
    root_process->task_state.r6 = 0;
    root_process->task_state.r7 = 0;
    root_process->task_state.r8 = 0;
    root_process->task_state.r9 = 0;
    root_process->task_state.r10 = 0;
    root_process->task_state.r11 = 0;
    root_process->task_state.r12 = 0;
    root_process->task_state.sp = (uint32)bootinfo->user_stack_ceiling;
    root_process->task_state.lr = 0;

    idle_process = LIST_HEAD (&free_process_list);
    
    KASSERT (idle_process != NULL);
    
    LIST_REM_HEAD (&free_process_list, free_entry);
    
    idle_process->exit_status = 0;
    idle_process->quanta_used = 0;
    idle_process->sched_policy = SCHED_IDLE;            
    idle_process->tickets = 0;
    idle_process->stride = 0;
    idle_process->remaining = 0;
    idle_process->pass = 0;
    idle_process->continuation_function = NULL;
    idle_process->virtualalloc_sz = 0;

    LIST_INIT (&idle_process->pending_handle_list);
    LIST_INIT (&idle_process->close_handle_list);

    idle_process->task_state.cpu = &cpu_table[0];
    idle_process->task_state.flags = 0;
    
    idle_process->task_state.pc = (uint32)idle_loop;
    idle_process->task_state.r0 = 0;
    idle_process->task_state.cpsr = cpsr_dnm_state | SYS_MODE | CPSR_DEFAULT_BITS;
    idle_process->task_state.r1 = 0;
    idle_process->task_state.r2 = 0;
    idle_process->task_state.r3 = 0;
    idle_process->task_state.r4 = 0;
    idle_process->task_state.r5 = 0;
    idle_process->task_state.r6 = 0;
    idle_process->task_state.r7 = 0;
    idle_process->task_state.r8 = 0;
    idle_process->task_state.r9 = 0;
    idle_process->task_state.r10 = 0;
    idle_process->task_state.r11 = 0;
    idle_process->task_state.r12 = 0;
    idle_process->task_state.sp = (uint32)idle_stack_top;
    idle_process->task_state.lr = 0;

    root_process->state = PROC_STATE_RUNNING;
    idle_process->state = PROC_STATE_READY;
    
    cpu->current_process = root_process;
    cpu->idle_process = idle_process;
    cpu->reschedule_request = 0;
    cpu->svc_stack = (vm_addr)&svc_stack_top;
    cpu->interrupt_stack = (vm_addr)&interrupt_stack_top;
    cpu->exception_stack = (vm_addr)&exception_stack_top;
        
    SchedReady (root_process);
}





/*
 *
 */
 
void InitRootAddressSpace (void)
{
    int t;  

    for (t=0; t<virtseg_cnt; t++)
    {
        if ((virtseg_table[t].flags & MEM_MASK) == MEM_ALLOC)
        {
            virtseg_table[t].owner = root_process;
            virtseg_table[t].version = segment_version_counter;
            segment_version_counter++;
        }
    }
}














    


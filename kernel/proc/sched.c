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
 * Scheduling related functions
 */

#include <kernel/types.h>
#include <kernel/proc.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>




/*
 * Picks the next process to run next. If a process's quanta has expired it
 * advances the appropriate schedule queue.
 *
 * The microkernel supports 2 scheduling policies.
 *
 * SCHED_RR is a 32-priority,round-robin scheduler for real-time or device
 * drivers.  These have priority over SCHED_OTHER processes.
 *
 * SCHED_OTHER is proportional fair share scheduler that uses the
 * Stride Scheduler algorithm of Carl A. Waldspurger as described in
 * "Lottery and Stride Scheduling: Flexible Proportional-Share Resource
 * Management"
 */
 
void Reschedule (void)
{
    struct Process *current, *next, *proc;
    struct CPU *cpu;
    int q;

    cpu = GetCPU();
    current = GetCurrentProcess();

    if (current->sched_policy == SCHED_RR)
    {
        CIRCLEQ_FORWARD (&realtime_queue[current->tickets], sched_entry);
        current->quanta_used = 0;
    }
    else if (current->sched_policy == SCHED_OTHER)
    {
        global_stride = STRIDE1 / global_tickets;
        global_pass += global_stride;
        current->pass += current->stride;
                
        LIST_REM_ENTRY (&stride_queue, current, stride_entry);
        proc = LIST_HEAD(&stride_queue);

        if (proc != NULL && global_pass < proc->pass)
            global_pass = proc->pass;

        while (proc != NULL)
        {
            if (proc->pass > current->pass)
            {
                LIST_INSERT_BEFORE (&stride_queue, proc, current, stride_entry);
                break;
            }
        
            proc = LIST_NEXT (proc, stride_entry);
        }
        
        if (proc == NULL)
            LIST_ADD_TAIL (&stride_queue, current, stride_entry);
        
        current->quanta_used = 0;
    }
    else if (current->sched_policy == SCHED_IDLE)
    {
    }

    
    next = NULL;

    if (realtime_queue_bitmap != 0)
    {
        for (q = 31; q >= 0; q--)
        {
            if ((realtime_queue_bitmap & (1<<q)) != 0)
                break;
        }
        
        next = CIRCLEQ_HEAD (&realtime_queue[q]);
    }
        
    if (next == NULL)
        next = LIST_HEAD (&stride_queue);
                            
    if (next == NULL)
        next = cpu->idle_process;
    
    current->state = PROC_STATE_READY;
    next->state = PROC_STATE_RUNNING;
    PmapSwitch (&next->pmap, &current->pmap);
    cpu->current_process = next;
}




/*
 * SchedReady();
 *
 * Add process to a ready queue based on its scheduling policy and priority.
 */

void SchedReady (struct Process *proc)
{
    struct Process *next;


    if (proc->sched_policy == SCHED_RR || proc->sched_policy == SCHED_FIFO)
    {
        CIRCLEQ_ADD_TAIL (&realtime_queue[proc->tickets], proc, sched_entry);
        realtime_queue_bitmap |= (1<<proc->tickets);
    }
    else if (proc->sched_policy == SCHED_OTHER)
    {
        global_tickets += proc->tickets;
        global_stride = STRIDE1 / global_tickets;
        proc->pass = global_pass - proc->remaining;
        
        next = LIST_HEAD(&stride_queue);
                
        while (next != NULL)
        {
            if (next->pass > proc->pass)
            {
                LIST_INSERT_BEFORE (&stride_queue, next, proc, stride_entry);
                break;
            }
                
            next = LIST_NEXT (next, stride_entry);
        }
                
        if (next == NULL)
        {
            LIST_ADD_TAIL (&stride_queue, proc, stride_entry);
        }
    }

    proc->quanta_used = 0;          
    reschedule_request = TRUE;
}




/*
 * SchedUnready();
 *
 * Removes process from the ready queue.  See SchedReady() above.
 */

void SchedUnready (struct Process *proc)
{
    if (proc->sched_policy == SCHED_RR || proc->sched_policy == SCHED_FIFO)
    {
        CIRCLEQ_REM_ENTRY (&realtime_queue[proc->tickets], proc, sched_entry);
        
        if (CIRCLEQ_HEAD (&realtime_queue[proc->tickets]) == NULL)
            realtime_queue_bitmap &= ~(1<<proc->tickets);
    }
    else if (proc->sched_policy == SCHED_OTHER)
    {
        global_tickets -= proc->tickets;
        proc->remaining = global_pass - proc->pass;
        LIST_REM_ENTRY (&stride_queue, proc, stride_entry);
    }
    
    proc->quanta_used = 0;
    reschedule_request = TRUE;
}




/*
 * Sets the scheduling policy, either round-robin or stride scheduler
 * and the priority/tickets of the child process 'h'.  If 'h' is -1
 * then the current process (the Executive) has its scheduling state
 * modified.
 *
 * This can only be called by the Executive process.
 */

SYSCALL int SetSchedParams (int h, int policy, int tickets)
{
    struct Process *current;
    struct Process *child;
    
    
    current = GetCurrentProcess();

    if (!(current->flags & PROCF_EXECUTIVE))
        return privilegeErr;

    if (h == -1)
        child = current;
    else if ((child = GetObject (current, h, HANDLE_TYPE_PROCESS)) == NULL)
        return paramErr;


    DisablePreemption();
        
    if (policy == SCHED_RR || policy == SCHED_FIFO)
    {
        if (tickets < 0 || tickets > 31)
            return paramErr;
    
        SchedUnready (child);
        child->sched_policy = policy;
        child->tickets = tickets;
        SchedReady (child);
        return 0;
    }
    else if (policy == SCHED_OTHER)
    {
        if (tickets < 0 || tickets > STRIDE_MAX_TICKETS)
            return paramErr;
    
        SchedUnready (child);
        child->sched_policy = policy;
        child->tickets = tickets;
        child->stride = STRIDE1 / child->tickets;
        child->remaining = child->stride;
        child->pass = global_pass;
        SchedReady (child);
        return 0;
    }
    else
    {
        return paramErr;
    }
}




/*
 * Yield();
 */

SYSCALL void Yield (void)
{
    struct Process *current;

    current = GetCurrentProcess();

    DisablePreemption();
    
    if (current->sched_policy == SCHED_RR || current->sched_policy == SCHED_FIFO)
    {
        CIRCLEQ_FORWARD (&realtime_queue[current->tickets], sched_entry);   
    }
}




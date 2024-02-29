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

//#define KDEBUG

#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>
#include <kernel/msg.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/arch.h>

// Static prototypes
static void TaskTimedSleepCallback(struct Timer *timer);


/* @brief   Perform a task switch
 *
 * Currently implements round-robin scheduling and a niceness scheduler.
 * Priorities 16 to 31 are for the round-robin scheduler. Priorities
 * 0 to 15 are for the niceness scheduler.
 *
 * The switching of the task's register context for another task's context
 * is done by the following magic:
 *
 * if (SetContext(&context[0]) == 0) {
 *   GetContext(next->context);
 * }
 * 
 * When SetContext is called, the current thread's registers are saved in
 * the context array on its stack. SetContext returns 0 to indicate it should
 * now get the context of the next stack.
 *
 * When SetContext stores the context of the current task it does something
 * perculiar, as mentioned it returns 0 but in the register state that it
 * saves it sets it so that a value of 1 would be returned, On ARM CPUs this
 * will be in register 0.  
 *
 * When GetContext is called the next thread's context is reloaded from its
 * stack into the CPU's registers. These registers are at the point they were saved
 * when it called SetContext. When the registers are reloaded they are what
 * was saved by SetContext so when it performs a return it is returning
 * from SetContext and not GetContext.
 * 
 * However, instead of returning 0 and performing the GetContext again
 * it returns the 1 stored by its original call to SetContext to prevent it
 * endlessly calling GetContext.
 *
 * At this point execution resumes after the "if" statement and the task is
 * switched.
 *
 * FIXME: Can we defer Fair-Share scheduling (e.g. lottery/stride) onto a DPC
 * scheduler task, like the timer task. Effectively this only schedules RTOS
 * round-robin tasks.  If no RTOS tasks, run the DPC task to choose a task.
 */
void Reschedule(void)
{
  context_word_t context[N_CONTEXT_WORD];
  struct Process *current, *next, *proc;
  struct CPU *cpu;
  int q;

  cpu = get_cpu();
  current = get_current_process();

  if (current != NULL) {
    current->quanta_used ++;
  
    if (current->sched_policy == SCHED_RR) {
      KASSERT(current->priority >= 16 && current->priority < 32);

      if ((CIRCLEQ_HEAD(&sched_queue[current->priority])) != NULL) {
        CIRCLEQ_FORWARD(&sched_queue[current->priority], sched_entry);
        current->quanta_used = 0;
      }
    } else if (current->sched_policy == SCHED_OTHER) {
      if (current->quanta_used == SCHED_QUANTA_JIFFIES) {      
        if (current->priority > 1) {
          CIRCLEQ_REM_HEAD(&sched_queue[current->priority], sched_entry);              
          current->priority--;
          CIRCLEQ_ADD_TAIL(&sched_queue[current->priority], current, sched_entry);        
        } else {
          CIRCLEQ_FORWARD(&sched_queue[current->priority], sched_entry);          
        }       
        current->quanta_used = 0;
      }     
    }
  }

  next = NULL;

  if (sched_queue_bitmap != 0) {
    for (q = 31; q >= 0; q--) {
      if ((sched_queue_bitmap & (1 << q)) != 0)
        break;
    }

    next = CIRCLEQ_HEAD(&sched_queue[q]);
  }

  if (next == NULL) {
    next = cpu->idle_process;
  }

  if (next != NULL) {
    next->state = PROC_STATE_RUNNING;
    pmap_switch(next, current);
  }

  if (next != current) {
    current->context = &context;
    cpu->current_process = next;

    if (SetContext(&context[0]) == 0) {
      GetContext(next->context);
    }
  }
}

/* @brief   Add process to a ready queue based on its scheduling policy and priority.
 */
void SchedReady(struct Process *proc)
{
  struct Process *next;
  struct CPU *cpu;

  cpu = get_cpu();

  if (proc->sched_policy == SCHED_RR || proc->sched_policy == SCHED_FIFO) {
    CIRCLEQ_ADD_TAIL(&sched_queue[proc->priority], proc, sched_entry);
    sched_queue_bitmap |= (1 << proc->priority);

  } else if (proc->sched_policy == SCHED_OTHER) {
    CIRCLEQ_ADD_TAIL(&sched_queue[proc->priority], proc, sched_entry);
    sched_queue_bitmap |= (1 << proc->priority);

  } else {
    Error("Ready: Unknown sched policy %d", proc->sched_policy);
    KernelPanic();
  }

  proc->quanta_used = 0;
  cpu->reschedule_request = true;
}


/* @brief   Removes process from the ready queue.
 */
void SchedUnready(struct Process *proc)
{
  struct CPU *cpu;

  cpu = get_cpu();

  if (proc->sched_policy == SCHED_RR || proc->sched_policy == SCHED_FIFO) {
    CIRCLEQ_REM_ENTRY(&sched_queue[proc->priority], proc, sched_entry);
    if (CIRCLEQ_HEAD(&sched_queue[proc->priority]) == NULL) {
      sched_queue_bitmap &= ~(1 << proc->priority);
    }

    proc->quanta_used = 0;
    
  } else if (proc->sched_policy == SCHED_OTHER) {
    CIRCLEQ_REM_ENTRY(&sched_queue[proc->priority], proc, sched_entry);
    if (CIRCLEQ_HEAD(&sched_queue[proc->priority]) == NULL) {
      sched_queue_bitmap &= ~(1 << proc->priority);
    }

    proc->priority = proc->desired_priority;
    proc->quanta_used = 0;

  } else {
    Error("Unready: Unknown sched policy *****");
    KernelPanic();
  }

  cpu->reschedule_request = true;
}


/* @brief   Set process scheduling policy and priority
 */
int sys_setschedparams(int policy, int priority)
{
  struct Process *current;
  current = get_current_process();

  if (policy == SCHED_RR || policy == SCHED_FIFO) {
    if (!(current->flags & PROCF_ALLOW_IO)) {
      return -EPERM;
    }

    if (priority < 16 || priority > 31) {
      return -EINVAL;
    }

    DisableInterrupts();
    SchedUnready(current);
    current->sched_policy = policy;
    current->desired_priority = priority;
    current->priority = priority;
    SchedReady(current);
    Reschedule();
    EnableInterrupts();

    return 0;

  } else if (policy == SCHED_OTHER) {
    if (priority < 0 || priority > 15) {
      return -EINVAL;
    }

    DisableInterrupts();
    SchedUnready(current);
    current->sched_policy = policy;
    current->desired_priority = priority;
    current->priority = priority;
    SchedReady(current);
    Reschedule();
    EnableInterrupts();

    return 0;
    
  } else {
    return -EINVAL;
  }
}


/* @brief   Lock the Big Kernel Lock
 *
 * Big Kernel Lock acquired on kernel entry. Effectively coroutining
 * (co-operative multitasking) within the kernel.  Similar to the mutex used in
 * a condition variable construct.  TaskSleep and TaskWakeup are used to sleep
 * and wakeup tasks blocked on a condition variable (rendez).
 * 
 * Interrupts are disabled upon entry to a syscall in the assembly code.
 */
void KernelLock(void)
{
  struct Process *current;

  current = get_current_process();

  if (bkl_locked == false) {
    bkl_locked = true;
    bkl_owner = current;
  } else {
    LIST_ADD_TAIL(&bkl_blocked_list, current, blocked_link);
    current->state = PROC_STATE_BKL_BLOCKED;
    SchedUnready(current);
    Reschedule();
  }
}


/* @brief   Unlock the Big Kernel Lock
 *
 * See comments above for KernelLock()
 *
 * This is called at the end of a system call or exception.  This checks to see
 * if there are any remaining processes blocked on the big kernel lock and if so
 * yields to one of these processes.
 *
 * Only when there are no other processes blocked on the big kernel lock do we
 * return to user mode.  
 *
 * We do not have kernel preemption, effectively all processes blocked on the BKL
 * must run before we can return to user space.
 */
void KernelUnlock(void)
{
  struct Process *proc;

  if (bkl_locked == true) {
    proc = LIST_HEAD(&bkl_blocked_list);   // Pick the next process that is blocked on bkl

    if (proc != NULL) {
      bkl_locked = true;      // It should be locked already by previous if statement
      bkl_owner = proc;

      LIST_REM_HEAD(&bkl_blocked_list, blocked_link);

      proc->state = PROC_STATE_READY;
      SchedReady(proc);       // 
      Reschedule();
    } else {
      bkl_locked = false;
      bkl_owner = (void *)0xcafef00d;
    }
  } else {
    KernelPanic();
  }
}


/* @brief   Initialize a Rendez condition variable
 *
 * @param   rendez, condition variable to initialize
 */
void InitRendez(struct Rendez *Rendez)
{ 
  LIST_INIT(&Rendez->blocked_list);
}


/* @brief   Sleep on a Rendez condition variable
 *
 * @param   rendez, condition variable to sleep on
 */
void TaskSleep(struct Rendez *rendez)
{
  struct Process *proc;
  struct Process *current;
  int_state_t int_state;
  
  current = get_current_process();

  int_state = DisableInterrupts();

  KASSERT(bkl_locked == true);
  KASSERT(bkl_owner == current);

  proc = LIST_HEAD(&bkl_blocked_list);

  if (proc != NULL) {
    LIST_REM_HEAD(&bkl_blocked_list, blocked_link);
    proc->state = PROC_STATE_READY;
    bkl_owner = proc;
    SchedReady(proc);
  } else {
    bkl_locked = false;
    bkl_owner = (void *)0xdeadbeef;
  }

  LIST_ADD_TAIL(&rendez->blocked_list, current, blocked_link);
  current->state = PROC_STATE_RENDEZ_BLOCKED;
  SchedUnready(current);
  Reschedule();

  KASSERT(bkl_locked == true);
  KASSERT(bkl_owner == current);

  RestoreInterrupts(int_state);
}


/* @brief   Sleep on a Rendez condition variable with a timeout.
 *
 * @param   rendez, condition variable to sleep on
 * @param   ts, timeout to wake up after if the rendez was not signalled
 * @return  0 on success
 *          -ETIMEDOUT if a timeout occured
 *          other negative errno on failure
 */
int TaskTimedSleep(struct Rendez *rendez, struct timespec *ts)
{
  struct Process *proc;
  struct Process *current;
  struct Timer *timer;
  int_state_t int_state;
  int sc;
  
  current = get_current_process();

  int_state = DisableInterrupts();

  KASSERT(bkl_locked == true);
  KASSERT(bkl_owner == current);

  proc = LIST_HEAD(&bkl_blocked_list);

  if (proc != NULL) {
    LIST_REM_HEAD(&bkl_blocked_list, blocked_link);
    proc->state = PROC_STATE_READY;
    bkl_owner = proc;
    SchedReady(proc);
  } else {
    bkl_locked = false;
    bkl_owner = (void *)0xdeadbeef;
  }
    
  timer = &current->sleep_timer;  
  timer->process = current;
  timer->arg = rendez;
  timer->armed = true;
  timer->callback = TaskTimedSleepCallback;

  SpinLock(&timer_slock);
  timer->expiration_time = hardclock_time + (ts->tv_sec * JIFFIES_PER_SECOND + ts->tv_nsec / NANOSECONDS_PER_JIFFY);
  SpinUnlock(&timer_slock);

  LIST_ADD_TAIL(&timing_wheel[timer->expiration_time % JIFFIES_PER_SECOND], timer, timer_entry);
  
  LIST_ADD_TAIL(&rendez->blocked_list, current, blocked_link);
  current->state = PROC_STATE_RENDEZ_BLOCKED;
  SchedUnready(current);
  Reschedule();

  if (timer->armed == true) {
    LIST_REM_ENTRY(&timing_wheel[timer->expiration_time % JIFFIES_PER_SECOND], timer, timer_entry);
    timer->armed = false;
    timer->process = NULL;
    timer->callback = NULL;
    sc = 0;
  } else {
    sc = -ETIMEDOUT;
  }

  RestoreInterrupts(int_state);
  return sc;
}


/* @brief   Timeout handler of task blocked by TaskTimedSleep()
 *
 * @param   timer, the timeout timer state created by TaskTimedSleep().
 *
 * This is called from the timer bottom half thread, so has the BKL
 * locked until it does a sleep
 */
static void TaskTimedSleepCallback(struct Timer *timer)
{
  struct Process *proc = timer->process;
  struct Rendez *rendez = timer->arg;
  int_state_t int_state;
  
  int_state = DisableInterrupts();

  if (proc != NULL && proc->state == PROC_STATE_RENDEZ_BLOCKED) {
    LIST_REM_ENTRY(&rendez->blocked_list, proc, blocked_link);
    LIST_ADD_TAIL(&bkl_blocked_list, proc, blocked_link);
    proc->state = PROC_STATE_BKL_BLOCKED;
  }
  
  RestoreInterrupts(int_state);
}


/* @brief   Wakeup a single task waiting on a condition variable
 *
 * @param   rendez, the condition variable to wake up a task from
 */
void TaskWakeup(struct Rendez *rendez)
{
  struct Process *proc;
  int_state_t int_state;
  
  int_state = DisableInterrupts();

  proc = LIST_HEAD(&rendez->blocked_list);

  if (proc != NULL) {
    LIST_REM_HEAD(&rendez->blocked_list, blocked_link);
    LIST_ADD_TAIL(&bkl_blocked_list, proc, blocked_link);
    proc->state = PROC_STATE_BKL_BLOCKED;
  }

  RestoreInterrupts(int_state);
}


/* @brief   Wakeup a blocked task from an interrupt handler
 *
 * @param   rendez, the condition variable to wake up a task from
 */
void TaskWakeupFromISR(struct Rendez *rendez)
{
  struct Process *proc;
  struct CPU *cpu;

  proc = LIST_HEAD(&rendez->blocked_list);

  if (proc != NULL) {
    LIST_REM_HEAD(&rendez->blocked_list, blocked_link);
    LIST_ADD_TAIL(&bkl_blocked_list, proc, blocked_link);
    proc->state = PROC_STATE_BKL_BLOCKED;
  }

//  cpu = get_cpu();  
//  cpu->reschedule_request = true;
}


/* @brief   Wakeup all tasks waiting on a condition variable
 *
 * @param   rendez, the condition variable to wake up all tasks from
 */
void  TaskWakeupAll(struct Rendez *rendez)
{
  struct Process *proc;
  int_state_t int_state;
  
  do {
    int_state = DisableInterrupts();

    proc = LIST_HEAD(&rendez->blocked_list);

    if (proc != NULL) {
      KASSERT(bkl_locked == true);
      
      LIST_REM_HEAD(&rendez->blocked_list, blocked_link);
      LIST_ADD_TAIL(&bkl_blocked_list, proc, blocked_link);
      proc->state = PROC_STATE_BKL_BLOCKED;
    }

    proc = LIST_HEAD(&rendez->blocked_list);
    RestoreInterrupts(int_state);

  } while (proc != NULL);
}



int sys_getpriority(void)
{
	return 0;
}

int sys_setpriority(void)
{
	return 0;
}

int sys_sysconf(void)
{
	Info ("sys_sysconf");
	return -ENOSYS;
}


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

#define KDEBUG

#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>
#include <kernel/msg.h>
#include <kernel/proc.h>
#include <kernel/types.h>


/*
 * Task Scheduler : Currently implements round-robin scheduling and a stride
 * scheduler which needs work. 
 */
void Reschedule(void) {
  uint32_t context[15];
  struct Process *current, *next, *proc;
  struct CPU *cpu;
  int q;

  cpu = get_cpu();
  current = get_current_process();

  if (current != NULL) {
    if (current->sched_policy == SCHED_RR) {
      KASSERT(current->rr_priority >= 0 && current->rr_priority < 32);

      if ((CIRCLEQ_HEAD(&realtime_queue[current->rr_priority])) != NULL) {
        CIRCLEQ_FORWARD(&realtime_queue[current->rr_priority], sched_entry);
        current->quanta_used = 0;
      }
    } else if (current->sched_policy == SCHED_OTHER) {
      global_stride = STRIDE1 / global_tickets;
      global_pass += global_stride;
      current->stride_pass += current->stride;

      LIST_REM_ENTRY(&stride_queue, current, stride_entry);
      proc = LIST_HEAD(&stride_queue);

      if (proc != NULL && global_pass < proc->stride_pass)
        global_pass = proc->stride_pass;

      while (proc != NULL) {
        if (proc->stride_pass > current->stride_pass) {
          LIST_INSERT_BEFORE(&stride_queue, proc, current, stride_entry);
          break;
        }

        proc = LIST_NEXT(proc, stride_entry);
      }

      if (proc == NULL) {
        LIST_ADD_TAIL(&stride_queue, current, stride_entry);
      }

      current->quanta_used = 0;
    } else if (current->sched_policy == SCHED_IDLE) {
    }
  }

  next = NULL;

  if (realtime_queue_bitmap != 0) {
    for (q = 31; q >= 0; q--) {
      if ((realtime_queue_bitmap & (1 << q)) != 0)
        break;
    }

    next = CIRCLEQ_HEAD(&realtime_queue[q]);
  }

  if (next == NULL) {
    next = LIST_HEAD(&stride_queue);
  }

  if (next == NULL) {
    next = cpu->idle_process;
  }

  if (next != NULL) {
    next->state = PROC_STATE_RUNNING;
    PmapSwitch(next, current);
  }

  if (next != current) {
    current->context = &context;
    cpu->current_process = next;

    if (SetContext(&context[0]) == 0) {
      GetContext(next->context);
    }
  }
}

/*
 * SchedReady();
 *
 * Add process to a ready queue based on its scheduling policy and priority.
 */
void SchedReady(struct Process *proc) {
  struct Process *next;
  struct CPU *cpu;

  cpu = get_cpu();

  if (proc->sched_policy == SCHED_RR || proc->sched_policy == SCHED_FIFO) {
    CIRCLEQ_ADD_TAIL(&realtime_queue[proc->rr_priority], proc, sched_entry);
    realtime_queue_bitmap |= (1 << proc->rr_priority);
  } else if (proc->sched_policy == SCHED_OTHER) {
    global_tickets += proc->stride_tickets;
    global_stride = STRIDE1 / global_tickets;
    proc->stride_pass = global_pass - proc->stride_remaining;

    next = LIST_HEAD(&stride_queue);

    while (next != NULL) {
      if (next->stride_pass > proc->stride_pass) {
        LIST_INSERT_BEFORE(&stride_queue, next, proc, stride_entry);
        break;
      }

      next = LIST_NEXT(next, stride_entry);
    }

    if (next == NULL) {
      LIST_ADD_TAIL(&stride_queue, proc, stride_entry);
    }
  } else {
    Error("Ready: Unknown sched policy %d", proc->sched_policy);
    KernelPanic();
  }

  proc->quanta_used = 0;
  cpu->reschedule_request = true;
}

/*
 * SchedUnready();
 *
 * Removes process from the ready queue.  See SchedReady() above.
 */
void SchedUnready(struct Process *proc) {
  struct CPU *cpu;

  cpu = get_cpu();

  if (proc->sched_policy == SCHED_RR || proc->sched_policy == SCHED_FIFO) {
    CIRCLEQ_REM_ENTRY(&realtime_queue[proc->rr_priority], proc, sched_entry);
    proc->sched_entry.next = NULL;
    proc->sched_entry.prev = NULL;    
    if (CIRCLEQ_HEAD(&realtime_queue[proc->rr_priority]) == NULL)
      realtime_queue_bitmap &= ~(1 << proc->rr_priority);
  } else if (proc->sched_policy == SCHED_OTHER) {
    global_tickets -= proc->stride_tickets;
    proc->stride_remaining = global_pass - proc->stride_pass;
    LIST_REM_ENTRY(&stride_queue, proc, stride_entry);
  } else {
    Error("Unready: Unknown sched policy *****");
    KernelPanic();
  }

  proc->quanta_used = 0;
  cpu->reschedule_request = true;
}

/*
 * Sets the scheduling policy, either round-robin or stride scheduler
 */
SYSCALL int sys_setschedparams(int policy, int priority) {
  struct Process *current;
  current = get_current_process();

  if (policy == SCHED_RR || policy == SCHED_FIFO) {
    if (!(current->flags & PROCF_ALLOW_IO)) {
      return -EPERM;
    }

    if (priority < 0 || priority > 31) {
      return -EINVAL;
    }

    DisableInterrupts();
    SchedUnready(current);
    current->sched_policy = policy;
    current->rr_priority = priority;
    SchedReady(current);
    Reschedule();
    EnableInterrupts();

    return 0;

  } else if (policy == SCHED_OTHER) {
    if (priority < 0 || priority > STRIDE_MAX_TICKETS) {
      return -EINVAL;
    }

    DisableInterrupts();
    SchedUnready(current);
    current->sched_policy = policy;
    current->stride_tickets = priority;
    current->stride = STRIDE1 / current->stride_tickets;
    current->stride_remaining = current->stride;
    current->stride_pass = global_pass;
    SchedReady(current);
    Reschedule();
    EnableInterrupts();

    return 0;
    
  } else {
    return -EINVAL;
  }
}

/*
 * Yield();
 */
SYSCALL void SysYield(void) {
}

/*
 * Big kernel lock acquired on kernel entry. Effectively coroutining
 * (co-operative multitasking) within the kernel.  Similar to the mutex used in
 * a condition variable construct.  TaskSleep and TaskWakeup are used to sleep
 * and wakeup tasks blocked on a condition variable (rendez).
 * 
 * Interrupts are disabled upon entry to a syscall in the assembly code.
 */
void KernelLock(void) {
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

/*
 *
 */
void KernelUnlock(void) {
  struct Process *proc;

  if (bkl_locked == true) {
    proc = LIST_HEAD(&bkl_blocked_list);

    if (proc != NULL) {
      bkl_locked = true;
      bkl_owner = proc;

      LIST_REM_HEAD(&bkl_blocked_list, blocked_link);

      proc->state = PROC_STATE_READY;
      SchedReady(proc);
      Reschedule();
    } else {
      bkl_locked = false;
      bkl_owner = (void *)0xcafef00d;
    }
  } else {
    KernelPanic();
  }
}

/*
 *
 */
void InitRendez(struct Rendez *Rendez)
{ 
  LIST_INIT(&Rendez->blocked_list);
}

/*
 *
 */
void TaskSleep(struct Rendez *rendez) {
  struct Process *proc;
  struct Process *current;

  current = get_current_process();

  DisableInterrupts();

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

  EnableInterrupts();
}

/*
 *
 */
void TaskWakeup(struct Rendez *rendez) {
  struct Process *proc;

  DisableInterrupts();

  proc = LIST_HEAD(&rendez->blocked_list);

  if (proc != NULL) {
    LIST_REM_HEAD(&rendez->blocked_list, blocked_link);
    LIST_ADD_TAIL(&bkl_blocked_list, proc, blocked_link);
    proc->state = PROC_STATE_BKL_BLOCKED;
  }

  EnableInterrupts();
}

/*
 * 
 */
void TaskWakeupFromISR(struct Rendez *rendez) {
  struct Process *proc;

  proc = LIST_HEAD(&rendez->blocked_list);

  if (proc != NULL) {
    LIST_REM_HEAD(&rendez->blocked_list, blocked_link);
    LIST_ADD_TAIL(&bkl_blocked_list, proc, blocked_link);
    proc->state = PROC_STATE_BKL_BLOCKED;
  }
}

/*
 *
 */
void  TaskWakeupAll(struct Rendez *rendez) {
  struct Process *proc;

  do {
    DisableInterrupts();

    proc = LIST_HEAD(&rendez->blocked_list);

    if (proc != NULL) {
      KASSERT(bkl_locked == true);
      
      LIST_REM_HEAD(&rendez->blocked_list, blocked_link);

      KASSERT(bkl_locked == true);

      LIST_ADD_TAIL(&bkl_blocked_list, proc, blocked_link);
      proc->state = PROC_STATE_BKL_BLOCKED;
    }

    proc = LIST_HEAD(&rendez->blocked_list);
    EnableInterrupts();

  } while (proc != NULL);
}


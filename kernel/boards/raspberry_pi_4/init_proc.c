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

#define KDEBUG  1

#include <kernel/arch.h>
#include <kernel/board/boot.h>
#include <kernel/board/globals.h>
#include <kernel/board/init.h>
#include <kernel/board/task.h>
#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/timer.h>
#include <kernel/utility.h>
#include <kernel/vm.h>
#include <string.h>
#include <sys/time.h>

// Variables
extern int svc_stack_top;
extern int interrupt_stack_top;
extern int exception_stack_top;



/* @brief Initialize process management data structures
 *
 * Initializes kernel tables relating to processes, ipc, scheduling and timers.
 * Starts the first process, root_process by calling SwitchToRoot().
 * This loads the FPU state, switches the stack pointer into the kernel
 * and reloads the page directory.  When this is done no lower memory which
 * includes this initialization code will be visible.
 */

void init_processes(void)
{
//  struct Timer *timer;
//  struct ISRHandler *isr_handler;
  struct CPU *cpu;
  struct Process *proc;

  Info("InitProcesses.,");
  
  bkl_locked = false;
  bkl_owner = NULL;
  LIST_INIT(&bkl_blocked_list);

  for (int t = 0; t < NIRQ; t++) {
    LIST_INIT(&isr_handler_list[t]);
  }

  for (int t = 0; t < 32; t++) {
    CIRCLEQ_INIT(&sched_queue[t]);
  }

  free_process_cnt = max_process;

  for (int t = 0; t < max_process; t++) {
    proc = GetProcess(t);
    proc->in_use = false;
  }

  Info(".. process table entries inited");
  
  for (int t = 0; t < JIFFIES_PER_SECOND; t++) {
    LIST_INIT(&timing_wheel[t]);
  }

  Info(".. timing wheel inited");

  InitRendez(&timer_rendez);
  softclock_time = hardclock_time = 0;

  max_cpu = 1;
  cpu_cnt = max_cpu;
  cpu = &cpu_table[0];
  cpu->reschedule_request = 0;
  cpu->svc_stack = (vm_addr)&svc_stack_top;
  cpu->interrupt_stack = (vm_addr)&interrupt_stack_top;
  cpu->exception_stack = (vm_addr)&exception_stack_top;

  Info(".. cpu struct inited");

  root_process =
      create_process(BootstrapRootProcess, SCHED_RR, 16, PROCF_USER, &cpu_table[0]);

  Info("root process created");

  timer_process =
      create_process(TimerBottomHalf, SCHED_RR, 31, PROCF_KERNEL, &cpu_table[0]);

  Info("timer process created");

//  interrupt_dpc_process =
//      create_process(interrupt_dpc, SCHED_RR, 30, PROCF_KERNEL, &cpu_table[0]);

  Info("interrupt dpc process created");

//  bdflush_process =
//      create_process(bdflush, SCHED_RR, 17, PROCF_KERNEL, &cpu_table[0]);

  Info("bdflush process created");

  // Can we not schedule a no-op bit of code if no processes running?
  // Do we really need an idle task in separate address-space ? 
  cpu->idle_process = create_process(Idle, SCHED_IDLE, 0, PROCF_KERNEL, &cpu_table[0]);

  Info("idle process created");

  // Pick root process to run,  Switch To Root here  
  root_process->state = PROC_STATE_RUNNING;
  cpu->current_process = root_process;

  Info("marking processes as initialized");

  ProcessesInitialized();


  Info("switching to root_process");

  pmap_switch(root_process, NULL);           

  Info("..switched to root_process pmap");

  Info("Calling GetContext(root_process)");
  
  // TODO: Can we free any bootsector pages and initial page table?

  GetContext(root_process->context);
}

/* @brief Create initial processes, root process and kernel processes for timer and idle tasks
 *
 * The address space created only has the kernel mapped. User-Space is marked as free.
 * The root process page directory is switched to in InitProcesses.
 */
struct Process *create_process(void (*entry)(void), int policy, int priority,
                               bits32_t flags, struct CPU *cpu)
{
  struct Process *proc;
  int pid;
  struct UserContext *uc;
  uint32_t *context;
  uint32_t cpsr;
  
  Info ("create_process..");
  
  proc = NULL;

  for (pid=0; pid < max_process; pid++) {
    proc = GetProcess(pid);
    
    if (proc->in_use == false) {
      break;
    }
  }

  if (proc == NULL) {
      Info ("create_process failed to find slot");
      return NULL;
  }

  memset(proc, 0, PROCESS_SZ);
  free_process_cnt--;

  InitRendez(&proc->rendez);
  LIST_INIT(&proc->child_list);

  proc->pid = pid;
  proc->parent = NULL;
  proc->in_use = true;  
  proc->state = PROC_STATE_INIT;
  proc->exit_status = 0;
  proc->log_level = 5;

  Info ("init_msgport(proc->reply_port)");
  
  init_msgport(&proc->reply_port);
  
  Info ("init_fproc(proc)");
 
  init_fproc(proc);
  
//  proc->fproc->current_dir = NULL;
//  proc->fproc->root_dir = NULL;

  Info ("Calling pmap_create()");

  // We create new page tables here for new root process.
  if (pmap_create(&proc->as) != 0) {
    return NULL;
  }
      
  proc->as.segment_cnt = 1;
  proc->as.segment_table[0] = VM_USER_BASE | SEG_TYPE_FREE;
  proc->as.segment_table[1] = VM_USER_CEILING | SEG_TYPE_CEILING;
  proc->cpu = cpu;

  proc->flags = flags;

  Info ("SchedReady() process");

  if (policy == SCHED_RR || policy == SCHED_FIFO) {
    proc->quanta_used = 0;
    proc->sched_policy = policy;
    proc->priority = (priority >= 16) ? priority : 16;
    proc->desired_priority = priority;
    SchedReady(proc);
  } else if (policy == SCHED_OTHER) {
    if (priority > 15) {
      priority = 15;
    }
    
    proc->quanta_used = 0;
    proc->sched_policy = policy;
    proc->priority = (priority <= 15) ? priority : 15;
    proc->desired_priority = priority;
    SchedReady(proc);
  } else if (policy == SCHED_IDLE) {
    proc->sched_policy = policy;
    proc->priority = 0;
    proc->desired_priority = 0;
  }

/* Move this into ArchInitExecProcess() IF returning to user mode. */
  Info ("Setting CPSR register user/sys mode");

#if 1
  if (proc->flags & PROCF_KERNEL) {
    cpsr = SYS_MODE | CPSR_DEFAULT_BITS;
  } else {
    cpsr = USR_MODE | CPSR_DEFAULT_BITS;
  }
#else
  if (proc->flags & PROCF_KERNEL) {
    cpsr = cpsr_dnm_state | SYS_MODE | CPSR_DEFAULT_BITS;
  } else {
    cpsr = cpsr_dnm_state | USR_MODE | CPSR_DEFAULT_BITS;
  }
#endif

  Info("create_process(pid:%d cpsr:%08x)", proc->pid, cpsr);
    
  Info("Setting up process register state");
  
  Info ("Setting createprocess cpsr :%08x", cpsr);
  
  uc = (struct UserContext *)((vm_addr)proc + PROCESS_SZ -
                              sizeof(struct UserContext));
  memset(uc, 0, sizeof(*uc));
  uc->pc = (uint32_t)0xdeadbeea;
  uc->cpsr = cpsr;
  uc->sp = (uint32_t)0xdeadbeeb;

// kernel save/restore context

  context = ((uint32_t *)uc) - 15;

  context[0] = (uint32_t)entry;

  for (int t = 1; t <= 12; t++) {
    context[t] = 0;
  }

  context[13] = (uint32_t)uc;
  context[14] = (uint32_t)StartKernelProcess;

  proc->context = context;
  proc->catch_state.pc = 0xdeadbeef;
  return proc;
}

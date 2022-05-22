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

#include <kernel/arch.h>
#include <kernel/arm/boot.h>
#include <kernel/arm/globals.h>
#include <kernel/arm/init.h>
#include <kernel/arm/task.h>
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
extern int idle_stack_top;
extern int bdflush_stack_top;




extern void BootstrapRootProcess(void);
extern void Idle(void);
extern void StartKernelProcess(void);
extern void TimerBottomHalf(void);



/* @brief Initialize process management data structures
 *
 * Initializes kernel tables relating to processes, ipc, scheduling and timers.
 * Starts the first process, root_process by calling SwitchToRoot().
 * This loads the FPU state, switches the stack pointer into the kernel
 * and reloads the page directory.  When this is done no lower memory which
 * includes this initialization code will be visible.
 */

void InitProcesses(void) {
//  struct Timer *timer;
//  struct ISRHandler *isr_handler;
  struct CPU *cpu;
  struct Process *proc;

  bkl_locked = false;
  bkl_owner = NULL;
  LIST_INIT(&bkl_blocked_list);

  for (int t = 0; t < NIRQ; t++) {
    LIST_INIT(&isr_handler_list[t]);
  }

  for (int t = 0; t < 32; t++) {
    CIRCLEQ_INIT(&realtime_queue[t]);
  }

  LIST_INIT(&stride_queue);

  free_process_cnt = max_process;

  for (int t = 0; t < max_process; t++) {
    proc = GetProcess(t);
    proc->in_use = false;
  }

  for (int t = 0; t < JIFFIES_PER_SECOND; t++) {
    LIST_INIT(&timing_wheel[t]);
  }

  InitRendez(&timer_rendez);
  softclock_time = hardclock_time = 0;

  max_cpu = 1;
  cpu_cnt = max_cpu;
  cpu = &cpu_table[0];
  cpu->reschedule_request = 0;
  cpu->svc_stack = (vm_addr)&svc_stack_top;
  cpu->interrupt_stack = (vm_addr)&interrupt_stack_top;
  cpu->exception_stack = (vm_addr)&exception_stack_top;

  root_process =
      CreateProcess(BootstrapRootProcess, SCHED_RR, 1, PROCF_USER, &cpu_table[0]);

  timer_process =
      CreateProcess(TimerBottomHalf, SCHED_RR, 31, PROCF_KERNEL, &cpu_table[0]);

  // Can we not schedule a no-op bit of code if no processes running?
  // Do we really need an idle task in separate address-space ? 
  cpu->idle_process = CreateProcess(Idle, SCHED_RR, 0, PROCF_KERNEL, &cpu_table[0]);

  // Pick root process to run,  Switch To Root here  
  root_process->state = PROC_STATE_RUNNING;
  cpu->current_process = root_process;

  ProcessesInitialized();

  PmapSwitch(root_process, NULL);           
  
  // TODO: We switch address space here.
  // TODO: Can we free any bootsector pages and initial page table?

  GetContext(root_process->context);
}

/* @brief Create initial processes, root process and kernel processes for timer and idle tasks
 *
 * The address space created only has the kernel mapped. User-Space is marked as free.
 * The root process page directory is switched to in InitProcesses.
 */
struct Process *CreateProcess(void (*entry)(void), int policy, int priority, bits32_t flags, struct CPU *cpu) {
  struct Process *proc;
  int pid;
  struct UserContext *uc;
  uint32_t *context;
  uint32_t cpsr;
  
  proc = NULL;

  for (pid=0; pid < max_process; pid++) {
    proc = GetProcess(pid);
    
    if (proc->in_use == false) {
      break;
    }
  }

  if (proc == NULL) {
      return NULL;
  }

  memset(proc, 0, PROCESS_SZ);
  free_process_cnt--;

  InitProcessHandles(proc);
  InitRendez(&proc->rendez);
  LIST_INIT(&proc->child_list);

  proc->pid = pid;
  proc->parent = NULL;
  proc->in_use = true;  
  proc->state = PROC_STATE_INIT;
  proc->exit_status = 0;
  proc->log_level = 5;
  proc->current_dir = NULL;
  proc->msg = NULL;


  // We create new page tables here for new root process.
  if (PmapCreate(&proc->as) != 0) {
    return NULL;
  }
      
  proc->as.segment_cnt = 1;
  proc->as.segment_table[0] = VM_USER_BASE | SEG_TYPE_FREE;
  proc->as.segment_table[1] = VM_USER_CEILING | SEG_TYPE_CEILING;
  proc->cpu = cpu;

  proc->flags = flags;

  if (policy == SCHED_RR || policy == SCHED_FIFO) {
    proc->quanta_used = 0;
    proc->sched_policy = policy;
    proc->rr_priority = priority;
    SchedReady(proc);
  } else if (policy == SCHED_OTHER) {
    proc->quanta_used = 0;
    proc->sched_policy = policy;
    proc->stride_tickets = priority;
    proc->stride = STRIDE1 / proc->stride_tickets;
    proc->stride_remaining = proc->stride;
    proc->stride_pass = global_pass;
    SchedReady(proc);
  } else if (policy == SCHED_IDLE) {
    proc->sched_policy = policy;
  }

/* Move this into ArchInitExecProcess() IF returning to user mode.
  if (proc->flags & PROCF_KERNEL) {
    cpsr = cpsr_dnm_state | SYS_MODE | CPSR_DEFAULT_BITS;
  } else {
    cpsr = cpsr_dnm_state | USR_MODE | CPSR_DEFAULT_BITS;
  }
*/

  cpsr = cpsr_dnm_state | USR_MODE | CPSR_DEFAULT_BITS;

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

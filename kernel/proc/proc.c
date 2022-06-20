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
 
#include <kernel/arm/elf.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/signal.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>
#include <string.h>
#include <sys/execargs.h>

/* @brief System call to fork the calling process
 *
 */
SYSCALL int sys_fork(void) {
  struct Process *current;
  struct Process *proc;

  current = get_current_process();

  if ((proc = AllocProcess()) == NULL) {
    return -ENOMEM;
  }

  ForkProcessHandles(proc, current);

  if (ArchForkProcess(proc, current) != 0) {
    FreeProcessHandles(proc);
    FreeProcess(proc);
    return -ENOMEM;
  }

  if (ForkAddressSpace(&proc->as, &current->as) != 0) {
    FreeProcessHandles(proc);
    FreeProcess(proc);
    return -ENOMEM;
  }

  DisableInterrupts();
  LIST_ADD_TAIL(&current->child_list, proc, child_link);
  proc->state = PROC_STATE_READY;
  SchedReady(proc);
  EnableInterrupts();

  return GetProcessPid(proc);
}

/* @brief System call to exit the calling process.
 */
SYSCALL void sys_exit(int status) {
  struct Process *current;
  struct Process *parent;
  struct Process *child;
  struct Process *proc;
  
  current = get_current_process();
  parent = current->parent;

  KASSERT (parent != NULL);

  current->exit_status = status;

  FreeProcessHandles(current);
  CleanupAddressSpace(&current->as);

  while ((child = LIST_HEAD(&current->child_list)) != NULL) {
    LIST_REM_HEAD(&current->child_list, child_link);

    if (child->state == PROC_STATE_ZOMBIE) {

      // Or attach to root
      FreeAddressSpace(&child->as);
      ArchFreeProcess(child);
      FreeProcess(child);
    } else {
      LIST_ADD_TAIL(&root_process->child_list, child, child_link);
      child->parent = root_process;
    }
  }

// TODO: Cancel any alarms to interrupt handlers

//	parent->usignal.sig_pending |= SIGFCHLD;	
//	parent->usignal.siginfo_data[SIGCHLD-1].si_signo = SIGCHLD;
//	parent->usignal.siginfo_data[SIGCHLD-1].si_code = 0;
//	parent->usignal.siginfo_data[SIGCHLD-1].si_value.sival_int = 0;

  if (current->pid == current->pgrp) {
	// Kill (-current->pgrp, SIGHUP);
  }
    
  TaskWakeup(&parent->rendez);

  DisableInterrupts();
  KASSERT(bkl_locked == true);
  
  if (bkl_owner != current) {
      Info ("bkl_owner = %08x", (vm_addr)bkl_owner);
      KASSERT(bkl_owner == current);
  }
  
  proc = LIST_HEAD(&bkl_blocked_list);

  if (proc != NULL) {
    LIST_REM_HEAD(&bkl_blocked_list, blocked_link);
    proc->state = PROC_STATE_READY;
    bkl_owner = proc;
    SchedReady(proc);
  } else {
    bkl_locked = false;
    bkl_owner = NULL;
  }

  current->state = PROC_STATE_ZOMBIE;
  SchedUnready(current);
  Reschedule();
  EnableInterrupts();
}

/* @brief System call to wait for child processes to exit
 */
SYSCALL int sys_waitpid(int pid, int *status, int options) {
  struct Process *current;
  struct Process *child;
  bool found = false;
  int found_in_pgrp = 0;
  int err = 0;
  
  current = get_current_process();

  if (-pid >= max_process || pid >= max_process) {
    return -EINVAL;
  }

  while (!found) // && pending_signals
  {
    if (pid > 0) {
      found = false;

      child = GetProcess(pid);
      if (child != NULL && child->in_use != false && child->parent == current) {
        if (child->state == PROC_STATE_ZOMBIE) {
          found = true;
        }
      } else {
        err = -EINVAL;
        goto exit;
      }
    } else if (pid == 0) {
      // Look for any child process that is a zombie and belongs to current
      // process group
      // check if any belong to current process group, exit if none.e

      child = LIST_HEAD(&current->child_list);

      if (child == NULL) {
        err = -EINVAL;
        goto exit;
      }

      found = false;
      found_in_pgrp = 0;

      while (child != NULL) {
        if (child->pgrp == current->pgrp) {
          found_in_pgrp++;

          if (child->state == PROC_STATE_ZOMBIE) {
            found = true;
            break;
          }
        }

        child = LIST_NEXT(child, child_link);
      }

      if (found_in_pgrp == 0) {
        err = -EINVAL;
        goto exit;
      }
    } else if (pid == -1) {
      // Look for any child process.

      child = LIST_HEAD(&current->child_list);

      if (child == NULL && current != root_process) {
        err = -EINVAL;        
        goto exit;
      }

      found = false;

      while (child != NULL) {
        if (child->state == PROC_STATE_ZOMBIE) {
          found = true;
          break;
        }

        child = LIST_NEXT(child, child_link);
      }
    } else {
      // Look for any child process with a specific process group id.
      // Check if any belongs to process group.

      child = LIST_HEAD(&current->child_list);

      if (child == NULL) {
        err = -EINVAL;
        goto exit;
      }

      found = false;
      found_in_pgrp = 0;

      while (child != NULL) {
        if (child->pgrp == -pid) {
          found_in_pgrp++;

          if (child->state == PROC_STATE_ZOMBIE) {
            found = true;
            break;
          }
        }

        child = LIST_NEXT(child, child_link);
      }

      if (found_in_pgrp == 0) {
        err = -EINVAL;
        goto exit;
      }
    }

    if (!found && (options & WNOHANG)) {
        err = -EAGAIN;
        goto exit;
    } else if (!found) {
      TaskSleep(&current->rendez);
    }
  }

  if (!found) // && pending_signals
  {
    err = -EINTR;
    goto exit;
  }

  pid = GetProcessPid(child);

  if (status != NULL) {
    current = get_current_process();

    if (CopyOut(status, &child->exit_status, sizeof *status) != 0) {
        err = -EFAULT;
        goto exit;
    }
  }

  LIST_REM_ENTRY(&current->child_list, child, child_link);

  FreeAddressSpace(&child->as);
  ArchFreeProcess(child);
  FreeProcess(child);
  
  return pid;

exit:
  return err;
}

/* @brief Allocate a process structure
 */
struct Process *AllocProcess(void) {
  struct Process *proc = NULL;
  struct Process *current;
  int pid;
    
  current = get_current_process();

  for (pid=0; pid < max_process; pid++) {
    proc = GetProcess(pid);
    
    if (proc->in_use == false) {
      break;
    }
  }

  if (pid == max_process) {
    return NULL;
  }
  
  memset(proc, 0, PROCESS_SZ);
  proc->in_use = true;
  proc->pid = pid;
  proc->parent = current;
  proc->state = PROC_STATE_INIT;
  proc->exit_status = 0;
  proc->flags = 0;
  proc->log_level = current->log_level;
  proc->quanta_used = current->quanta_used;
  proc->sched_policy = current->sched_policy;
  proc->rr_priority = current->rr_priority;
  proc->stride_tickets = current->stride_tickets;
  proc->stride = current->stride;
  proc->stride_pass = global_pass;
  proc->msg = NULL;

// FIXME: SigInit(proc);

  InitProcessHandles(proc);
  InitRendez(&proc->rendez);
  LIST_INIT(&proc->child_list);

  return proc;
}

/* @brief Free a process structure
 */
void FreeProcess(struct Process *proc) {
  proc->in_use = false;
}

/* @brief Get the process structure of the calling process
 */
struct Process *GetProcess(int pid) {
  if (pid < 0 || pid >= max_process) {
    return NULL;
  }

  return (struct Process *)((uint8_t *)process_table + (pid * PROCESS_SZ));
}

/* @brief Get the process ID of a process
 */
int GetProcessPid(struct Process *proc)
{
  return proc->pid;
}

/* @brief Get the Process ID of the calling process
 */
int GetPid(void)
{
  struct Process *current;
  
  current = get_current_process();
  return current->pid;
}



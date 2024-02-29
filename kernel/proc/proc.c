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

//#define KDEBUG

#include <kernel/board/elf.h>
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
#include <sys/wait.h>


/* @brief   Fork the calling process
 *
 * @return  In parent process, a positive non-zero Process ID of the child process
 *          In child process, 0 is returned
 *          On error a negative errno value is returned.
 */
int sys_fork(void)
{
  struct Process *current;
  struct Process *proc;

  current = get_current_process();

  if ((proc = AllocProcess()) == NULL) {
    return -ENOMEM;
  }

  fork_process_fds(proc, current);

  if (arch_fork_process(proc, current) != 0) {
    fini_fproc(proc);
    FreeProcess(proc);
    return -ENOMEM;
  }

  if (fork_address_space(&proc->as, &current->as) != 0) {
    fini_fproc(proc);
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


/* @brief   Exit the current process.
 * 
 * @param   Exit status to return to parent
 */
void sys_exit(int status)
{
  struct Process *current;
  struct Process *parent;
  struct Process *child;
  struct Process *proc;
  
  Info("sys_exit(%d)", status);
  
  current = get_current_process();
  parent = current->parent;

  KASSERT (parent != NULL);

  current->exit_status = status;

  fini_fproc(current);
  cleanup_address_space(&current->as);

  while ((child = LIST_HEAD(&current->child_list)) != NULL) {
    LIST_REM_HEAD(&current->child_list, child_link);

    if (child->state == PROC_STATE_ZOMBIE) {

      // Or attach to root
      free_address_space(&child->as);
      arch_free_process(child);
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
	  // FIXME: Kill (-current->pgrp, SIGHUP);
  }
    
  TaskWakeup(&parent->rendez);

  DisableInterrupts();
  KASSERT(bkl_locked == true);
  
  if (bkl_owner != current) {
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


/* @brief   Wait for child processes to exit
 *
 * @param   pid,
 * @param   status,
 * @param   options
 * @return  PID of exited child process or negative errno on failure  
 */
int sys_waitpid(int pid, int *status, int options)
{
  struct Process *current;
  struct Process *child;
  bool found = false;
  int found_in_pgrp = 0;
  int err = 0;
  
  Info("sys_waitpid(pid:%d, opt:%08x", pid, options);
  
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

  free_address_space(&child->as);
  arch_free_process(child);
  FreeProcess(child);
  
  return pid;

exit:
  return err;
}


/* @brief   Allocate a process structure
 *
 * @return  Allocated and initialized Process structure or NULL
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
  proc->quanta_used = 0;
  proc->sched_policy = current->sched_policy;
  proc->priority = current->priority;
  proc->desired_priority = current->desired_priority;
  
  // FIXME: SigInit(proc);
  init_msgport(&proc->reply_port);
  init_fproc(proc);
  
  InitRendez(&proc->rendez);
  LIST_INIT(&proc->child_list);

  return proc;
}

 
/* @brief   Free a process structure
 *
 * @param   proc, Process structure to free
 */
void FreeProcess(struct Process *proc)
{
  proc->in_use = false;
}


/* @brief   Get the process structure of the calling process
 * 
 * @param   pid, process ID of process to lookup
 * @return  Pointer to looked up process or NULL if it doesn't exist.
 */
struct Process *GetProcess(int pid)
{
  if (pid < 0 || pid >= max_process) {
    return NULL;
  }

  return (struct Process *)((uint8_t *)process_table + (pid * PROCESS_SZ));
}


/* @brief   Get the process ID of a process
 * 
 * @param   proc, Process structure to get PID of
 * @return  PID of process
 */
int GetProcessPid(struct Process *proc)
{
  return proc->pid;
}


/* @brief   Get the Process ID of the calling process
 *
 * @return  PID of calling process
 */
int GetPid(void)
{
  struct Process *current;
  
  current = get_current_process();
  return current->pid;
}



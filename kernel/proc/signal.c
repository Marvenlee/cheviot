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
 * Signal handling syscalls and functions. Copied from Kielder's sources.
 * TODO: Update signal handling to work on ARM processor.  We need the
 * libc code updating to handle signals too.
 */

#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/signal.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>
#include <signal.h>


/* @brief   Define the default actions and properties of each signal.
 */
const uint32_t sigprop[NSIG] = {
	0,		                          /* unused            */						              
	SP_KILL,                        /* 1  [ 0] SIGHUP    */
	SP_KILL,                        /* 2  [ 1] SIGINT    */
	SP_KILL|SP_CORE,                /* 3  [ 2] SIGQUIT   */
	SP_KILL|SP_CORE|SP_NORESET,     /* 4  [ 3] SIGILL    */
	SP_KILL|SP_CORE|SP_NORESET,     /* 5  [ 4] SIGTRAP   */
	SP_KILL|SP_CORE,                /* 6  [ 5] SIGABRT   */
	SP_KILL|SP_CORE,                /* 7  [ 6] SIGEMT    */
	SP_KILL|SP_CORE,                /* 8  [ 7] SIGFPE    */
	SP_KILL|SP_CANTMASK,            /* 9  [ 8] SIGKILL   */
	SP_KILL|SP_CORE,                /* 10 [ 9] SIGBUS    */
	SP_KILL|SP_CORE,                /* 11 [10] SIGSEGV   */
	SP_KILL|SP_CORE,                /* 12 [11] SIGSYS    */
	SP_KILL,                        /* 13 [12] SIGPIPE   */
	SP_KILL,                        /* 14 [13] SIGALRM   */
	SP_KILL,                        /* 15 [14] SIGTERM   */
	0,                              /* 16 [15] SIGURG    */
	SP_STOP|SP_CANTMASK,            /* 17 [16] SIGSTOP   */
	SP_STOP|SP_TTYSTOP,             /* 18 [17] SIGTSTP   */
	SP_CONT,                        /* 19 [18] SIGCONT   */
	0,                              /* 20 [19] SIGCHLD   */
	SP_STOP|SP_TTYSTOP,             /* 21 [20] SIGTTIN   */
	SP_STOP|SP_TTYSTOP,             /* 22 [21] SIGTTOU   */
	0,                              /* 23 [22] SIGIO     */
	SP_KILL,                        /* 24 [23] SIGXCPU   */
	SP_KILL,                        /* 25 [24] SIGXFSZ   */
	SP_KILL,                        /* 26 [25] SIGVTALRM */
	SP_KILL,                        /* 27 [26] SIGPROF   */
	0,					                    /* 28 [27] SIGWINCH  */
	0,						                  /* 29 [28] SIGINFO   */
	SP_KILL,						            /* 30 [29] SIGUSR1   */
	SP_KILL,						            /* 31 [30] SIGUSR2   */
};


/* @brief   Exit a process due to a signal
 */
void SigExit(int signal)
{
  sys_exit(signal<<8);  // TODO: FIXME signal exit
}


/* @brief   Copy signal handler settings to a new process
 */ 
void SigFork(struct Process *src, struct Process *dst)
{
	for (int t=1; t<NSIG; t++) {
		dst->signal.handler[t-1] = src->signal.handler[t-1];
		dst->signal.handler_mask[t-1] = src->signal.handler_mask[t-1];		
		dst->signal.si_code[t-1] = 0;
	}
	
	dst->signal.sig_info      = src->signal.sig_info;
	dst->signal.sig_mask      = src->signal.sig_mask;
	dst->signal.sig_pending   = 0;
	dst->signal.sig_resethand = src->signal.sig_resethand;
	dst->signal.sig_nodefer   = src->signal.sig_nodefer;
	dst->signal.restorer    = src->signal.restorer;	
	dst->signal.sigsuspend_oldmask = 0;
	dst->signal.use_sigsuspend_mask = false;
}


/* @brief   Initialize the signal handlers of a process
 */
void SigInit(struct Process *dst)
{
	for (int t=1; t<NSIG; t++) {
		dst->signal.handler[t-1] = SIG_DFL;
		dst->signal.handler_mask[t-1] = 0x00000000;		
		dst->signal.si_code[t-1] = 0;
	}
	
	dst->signal.sig_info      = 0x00000000;
	dst->signal.sig_mask      = 0x00000000;
	dst->signal.sig_pending   = 0x00000000;
	dst->signal.sig_resethand = 0x00000000;
	dst->signal.sig_nodefer   = 0x00000000;
	dst->signal.restorer    = NULL;
	dst->signal.sigsuspend_oldmask = 0;
	dst->signal.use_sigsuspend_mask = false;	
}


/* @brief   Update signal handlers due to a process exec'ing
 *
 */
void SigExec(struct Process *dst)
{
	for (int t=1; t<NSIG; t++) {
		if (dst->signal.handler[t-1] != SIG_IGN) {
			dst->signal.handler[t-1] = SIG_DFL;
    }
    
		dst->signal.handler_mask[t-1] = 0x00000000;
		dst->signal.si_code[t-1] = 0;		
	}
	
	dst->signal.sig_info      = 0x00000000;
	dst->signal.sig_resethand = 0x00000000;
	dst->signal.sig_nodefer   = 0x00000000;
	dst->signal.restorer    = NULL;
	dst->signal.sigsuspend_oldmask = 0;
	dst->signal.use_sigsuspend_mask = false;	
}


/* @brief   Configure signal handling of a process
 *
 * TODO: Add sa_restorer pointer for trampoline address and
 * SA_RESTORER flag to initialize it.
 * current_process->signal.trampoline = act_in->sa_restorer.
 */
int sys_sigaction(int signal, const struct sigaction *act_in, struct sigaction *oact_out)
{
	struct sigaction act, oact;
	struct Process *current;
	
	current = get_current_process();	

	if (signal <= 0 || signal >= NSIG) {
	  return -EINVAL;
	}
	
	if (oact_out != NULL) {
		if (current->signal.sig_info &= SIGBIT(signal))
			oact.sa_flags |= SA_SIGINFO;
			
		oact._signal_handlers._handler = (void *) current->signal.handler[signal-1];
		oact.sa_mask = current->signal.handler_mask[signal-1];

		if (CopyOut (oact_out, &oact, sizeof (struct sigaction)) != 0) {
		  return -EFAULT;
		}
	}
	
	if (act_in != NULL) {
		if (CopyIn(&act, act_in, sizeof (struct sigaction)) != 0) {
			return -EFAULT;
    }

		if (signal == SIGKILL || signal == SIGSTOP) {
			return -EINVAL;
		}

		if (act.sa_flags & SA_SIGINFO) {
			current->signal.sig_info |= SIGBIT(signal);
		}
		
		if (act.sa_flags & SA_NODEFER) {
			current->signal.sig_nodefer |= SIGBIT(signal);
		}
    
		if (act.sa_flags & SA_RESETHAND) {
			current->signal.sig_resethand |= SIGBIT(signal);
		}

    // FIXME: Ummm, when did we add this?  Rename to sa_restorer
    if (act.sa_flags & SA_RESTORER) {
      current->signal.restorer = (void (*)(void)) act.sa_restorer;
    }

    current->signal.handler[signal-1] = (void *) act._signal_handlers._handler;
    current->signal.handler_mask[signal-1] = act.sa_mask;
    current->signal.handler_mask[signal-1] &= ~(SIGBIT(SIGKILL) | SIGBIT(SIGSTOP));
	}
	
	return 0;
}


/* @brief   Send a signal to a process
 *
 * @param   pid, ID of target process to send signal to 
 * @param   signal, signal number to send to target process
 * @return  0 on success, negative errno on failure
 */
int sys_kill(int pid, int signal)
{
	struct Process *proc;
	int rc = 0;	

	if (signal <= 0 || signal >= NSIG || pid == 0) {
	  return -EINVAL;
	}
	
	if (pid > 0) {
		proc = GetProcess (pid);
    
    if (proc == NULL || proc->state == PROC_STATE_UNALLOC) {
			return -EINVAL;
		}

		DisableInterrupts();
		
		if (proc->signal.handler[signal-1] != SIG_IGN) {
			proc->signal.sig_pending |= SIGBIT (signal);
			proc->signal.si_code[signal-1] = SI_USER;

			if (proc->state == PROC_STATE_RENDEZ_BLOCKED && (proc->signal.sig_pending & proc->signal.sig_mask)) {				
				proc->state = PROC_STATE_READY;
				SchedReady (proc);
				Reschedule();
			}
		}
		
		EnableInterrupts();
	
	}	else {
		pid = -pid;
		
		for (int t=0; t < max_process; t++) {
			proc = GetProcess(t);

      if (proc == NULL) {
          continue;
      }

			DisableInterrupts();

			if (proc->state != PROC_STATE_UNALLOC) {
			  /* FIXME: WAS && proc->pid != pid) */ 
				if (proc->pgrp == pid) {
					if (proc->signal.handler[signal-1] != SIG_IGN) {
						proc->signal.sig_pending |= SIGBIT(signal);
      			proc->signal.si_code[signal-1] = SI_USER;

						if (proc->state == PROC_STATE_RENDEZ_BLOCKED
						    && (proc->signal.sig_pending & proc->signal.sig_mask)) {	
						  // TODO: What else needs to be done to wake up rendez?
						  // Add a Wakeup(proc) function ?
							proc->state = PROC_STATE_READY;
							SchedReady (proc);
							Reschedule();
						}
					}
				}
			}
			
			EnableInterrupts();
		}
	}

	return rc;
}


/* @brief   Wait for a signal to occur.
 *
 * @param   mask_in, controls which signals are to be accepted (0) or ignored (1).
 * @return  0 on success, negative errno on failrure
 */
int sys_sigsuspend(const sigset_t *mask_in)
{
	sigset_t mask;
	uint32_t intstate;
	struct Process *current;
	
	current = get_current_process();
	
	if (CopyIn(&mask, mask_in, sizeof (sigset_t)) != 0) {
	  return -EFAULT;
	}
	
	DisableInterrupts();

	current->signal.sigsuspend_oldmask = current->signal.sig_mask;
	current->signal.use_sigsuspend_mask = true;
	
	current->signal.sig_mask = mask;

	if ((current->signal.sig_pending & ~current->signal.sig_mask) == 0) {
	  TaskSleep(&current->rendez);
	}
	
	EnableInterrupts();	
	return 0;
}


/*
 *
 */
int sys_sigprocmask (int how, const sigset_t *set_in, sigset_t *oset_out)
{
	sigset_t set;
	struct Process *current;
	
	current = get_current_process();
	
	if (oset_out != NULL) {
		if (CopyOut (oset_out, &current->signal.sig_mask, sizeof (sigset_t)) != 0) {
		  return -EFAULT;
		}
  }
  
	if (set_in == NULL) {
	  return 0;
	}
	
	if (CopyIn (&set, set_in, sizeof (sigset_t)) == 0) {
    return -EFAULT;
	}
	
	if (how == SIG_SETMASK) {
		current->signal.sig_mask = set;
	} else if (how == SIG_BLOCK) {
		current->signal.sig_mask |= set;
  } else if (how == SIG_UNBLOCK) {
		current->signal.sig_mask &= ~set;
  } else {
		return -1;
	}
	
	return 0;
}


/* @brief   Return the set of pending signals
 * 
 * @param   set_out, Address to store the set of pending signals
 * @return  0 on success, negative errno on failure
 */
int sys_sigpending(sigset_t *set_out)
{
	sigset_t set;
	struct Process *current;
	
	current = get_current_process();
	
	set = current->signal.sig_pending & ~current->signal.sig_mask;	

	if (CopyOut (set_out, &set, sizeof (sigset_t)) != 0) {
	  return -EFAULT;
	}
	
	return 0;
}


/* @brief   Perform the default action of a received signal
 */ 
void DoSignalDefault (int sig)
{
	if (sigprop[sig] & SP_KILL) {
		SigExit (sig);
	}
}



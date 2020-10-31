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
 * KernelExit function.  Called prior to returning to user-mode by
 * Software Interrupts (SWI), interrupts and exceptions.  Also the
 * path that is taken whenever a process is put to sleep in the middle
 * of a system call.
 */

#include <kernel/arm/arm.h>
#include <kernel/arm/globals.h>
#include <kernel/arm/task.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/proc.h>
#include <kernel/signal.h>
#include <kernel/types.h>
#include <ucontext.h>


/*
 * PickSignal();
 */
int PickSignal (uint32_t sigbits)
{
	int sig;

	for (sig = 0; sig < 32; sig++) {
		if ((1<<sig) & sigbits) {
			break;
    }
  }
	return sig + 1;
}

/*
 * SigReturn();
 */
void SigReturn (struct sigframe *u_sigframe)
{
	struct sigframe sigframe;
	struct Process *current;
	struct UserContext *uc;
	
	current = GetCurrentProcess();
	
	uc = (struct UserContext *)((vm_addr)current + PROCESS_SZ - sizeof(struct UserContext));

	if (CopyIn (&sigframe, u_sigframe, sizeof (struct sigframe)) != 0) {	
		SigExit (SIGSEGV);
	}
	
	/*	
		state->eax = sigframe.sf_uc.uc_mcontext.eax;
		state->ebx = sigframe.sf_uc.uc_mcontext.ebx;
		state->ecx = sigframe.sf_uc.uc_mcontext.ecx;
		state->edx = sigframe.sf_uc.uc_mcontext.edx;
		state->esi = sigframe.sf_uc.uc_mcontext.esi;
		state->edi = sigframe.sf_uc.uc_mcontext.edi;
		state->ebp = sigframe.sf_uc.uc_mcontext.ebp;
		
		state->gs = PL3_DATA_SEGMENT;
		state->fs = PL3_DATA_SEGMENT;
		state->es = PL3_DATA_SEGMENT;
		state->ds = PL3_DATA_SEGMENT;
		
		state->return_eip    = sigframe.sf_uc.uc_mcontext.eip;
		state->return_cs     = PL3_CODE_SEGMENT;
		
		state->return_eflags = (sigframe.sf_uc.uc_mcontext.eflags & ~EFLAGS_SYSTEM_MASK)
								| (state->return_eflags & EFLAGS_SYSTEM_MASK)
								| EFLG_IF;
		
		state->return_esp    = sigframe.sf_uc.uc_mcontext.esp;
		state->return_ss     = PL3_DATA_SEGMENT;
	*/	
						
	current->signal.sig_mask = sigframe.sf_uc.uc_sigmask;
}

/*
 * CheckSignals();
 *
 * Called just before returning from a syscall, interrupt or exception by the
 * wrappers in the stubs.s file.  Only called if we are returning to PL3 and not
 * to virtual-86 mode.
 *
 * Interrupts are disabled for the duration of this function, though it should
 * only need slight modification to enable interrupts for most of it.
 *
 * SigExit() is responsible for enabling interrupts if a signal cannot be
 * delivered or it's default action is to kill the process.
 *
 * If we are returning from USigSuspend() then the process's current sigmask
 * will have been changed and the old mask saved.  We use the current sigmask
 * to determine what signal to deliver then reset it to the mask used prior
 * to calling USigSuspend().  See USigSuspend() for details.
 *
 * TODO: Copied from Kielder's x86 sources. Implement signal handling on ARM.
 */
void CheckSignals(struct UserContext *uc)
{
	struct sigframe sigframe;
	struct sigframe *u_sigframe;
	sigset_t safe_signals;
	sigset_t sync_signals;
	sigset_t old_mask;
	int sig;
	struct Process *current;
	
	current = GetCurrentProcess();
	
	sync_signals = current->signal.sig_pending & SYNCSIGMASK;

	if (sync_signals != 0 && (sync_signals & current->signal.sig_mask) != 0) {
		sig = PickSignal (sync_signals & ~current->signal.sig_mask);
		SigExit (sig);
	}
		
	safe_signals = current->signal.sig_pending & ~current->signal.sig_mask;
		
	if (safe_signals)	{	
		if (current->signal.use_sigsuspend_mask == true) {
			current->signal.sig_mask = current->signal.sigsuspend_oldmask;
			current->signal.use_sigsuspend_mask = false;
		}
			
		if (safe_signals & SIGBIT(SIGKILL)) {
			SigExit (SIGKILL);	
		
		}	else if (safe_signals != SIGBIT(SIGCONT)) {
			sig = PickSignal (safe_signals);
				
			current->signal.sig_pending &= ~SIGBIT(sig);
			
			old_mask = current->signal.sig_mask;
			
			if (current->signal.sig_nodefer & SIGBIT(sig)) {
				current->signal.sig_mask |= ~current->signal.handler_mask[sig-1];
			} else {
				current->signal.sig_mask |= SIGBIT(sig) | ~current->signal.handler_mask[sig-1];
			}
			
			if (current->signal.trampoline == NULL) {
				DoSignalDefault (sig);
			}	else if (current->signal.handler[sig-1] == SIG_DFL) {			
				DoSignalDefault (sig);				
				current->signal.sig_mask = old_mask;
			}	else if (current->signal.handler[sig-1] == SIG_IGN) {
				current->signal.sig_mask = old_mask;

				if (current->signal.sig_resethand & SIGBIT(sig)) {
					current->signal.handler[sig-1] = SIG_DFL;
				}
				
			} else {

                // copy context FIXME:
				
				sigframe.sf_uc.uc_sigmask = old_mask;
				
//				u_sigframe = (struct sigframe *) ALIGN_DOWN (state->return_esp - sizeof (struct sigframe), 16);
				sigframe.sf_signum = sig;
				
				if (current->signal.sig_info & SIGBIT(sig)) {
					sigframe.sf_siginfo = &u_sigframe->sf_si;
				} else {
					sigframe.sf_siginfo = NULL;
				}
				
				sigframe.sf_ucontext = &u_sigframe->sf_uc;
				sigframe.sf_ahu.sf_action = current->signal.handler[sig-1];
												
				if (current->signal.sig_resethand & SIGBIT(sig)) {
					current->signal.handler[sig-1] = SIG_DFL;
        }
        
				if (CopyOut (u_sigframe, &sigframe, sizeof (struct sigframe)) == 0) {		
//					state->return_esp = (uint32_t) u_sigframe;
//					state->return_eip = (uint32_t) current->signal.trampoline;
				}	else {
					SigExit (SIGSEGV);
				}
			}
		}
	}
}


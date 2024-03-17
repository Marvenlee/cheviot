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

#include <kernel/board/arm.h>
#include <kernel/board/globals.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/signal.h>
#include <kernel/timer.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>
#include <sys/signal.h>
#include <ucontext.h>


/*
 *
 */
void reset_handler(void)
{
  KernelPanic();
}


/*
 *
 */
void reserved_handler(void)
{
  Info("reserved_handler");
  KernelPanic();
}


/*
 *
 */
void fiq_handler(void)
{
  Info("fiq_handler");
  KernelPanic();
}


/*
 *
 */
void sys_unknownsyscallhandler(struct UserContext *context)
{
  struct Process *current;

	Error("Unknown syscall called");
  
  current = get_current_process();
  current->signal.si_code[SIGSYS-1] = 0;
  sys_kill(current->pid, SIGSYS);
}


/*
 *
 */
void sys_deprecatedsyscall(void)
{
  struct Process *current;

	Error("deprecated syscall called");

  current = get_current_process();
  current->signal.si_code[SIGSYS-1] = 0;
  sys_kill(current->pid, SIGSYS);
}


/*
 *
 */

void undef_instr_handler(struct UserContext *context)
{
  struct Process *current;

  Info("undef_instr_handler");
    
  current = get_current_process();

  if ((context->cpsr & CPSR_MODE_MASK) != USR_MODE &&
      (context->cpsr & CPSR_MODE_MASK) != SYS_MODE) {
    KernelPanic();
  }

  current->signal.si_code[SIGILL-1] = 0;
  current->signal.sigill_ptr = context->pc;
  sys_kill(current->pid, SIGILL);
}

/*
 *
 */

void prefetch_abort_handler(struct UserContext *context)
{
  vm_addr fault_addr;
  uint32_t mode;
  struct Process *current;
  
  current = get_current_process();

//  Info("prefetch_abort_handler");

  fault_addr = context->pc;

  mode = context->cpsr & CPSR_MODE_MASK;
  if (mode == USR_MODE || mode == SYS_MODE) {
    KernelLock();
  } else if (bkl_owner != current) {
    DisableInterrupts();
    PrintUserContext(context);
    Error("Prefetch Abort bkl not owner, fault addr = %08x", fault_addr);
    KernelPanic();
  } else {
    DisableInterrupts();
    PrintUserContext(context);
    Error("Prefetch Abort in kernel, fault addr = %08x", fault_addr);
    KernelPanic();
  }

  if (page_fault(fault_addr, PROT_EXEC) != 0) {
    if (mode == USR_MODE || mode == SYS_MODE) {
      PrintUserContext(context);
      Error("Prefetch Abort: fault addr = %08x", fault_addr);
      Error("Stack:");
      PrintMemDump(context->sp, 32);
      Error("PC:");
      PrintMemDump(context->pc, 32);
      Error("mode = %08x", mode);
      KernelPanic();
/*      
      current->signal.si_code[SIGSEGV-1] = 0;    // TODO: Could be access bit?
      current->signal.sigsegv_ptr = fault_addr;
      sys_kill(current->pid, SIGSEGV);
*/       
    } else {
      PrintUserContext(context);
      Error("In unexpected processor mode, fault addr = %08x", fault_addr);
      KernelPanic();
    }
  }



  KASSERT(context->pc != 0);

  if (mode == USR_MODE || mode == SYS_MODE) {
    CheckSignals(context);

    if (bkl_locked == false) {
      DisableInterrupts();
      PrintUserContext(context);
      Error("BKL is not locked when returning from prefetch page fault");
      KernelPanic();
    }
    KernelUnlock();
  }
}

/*
 * Page fault when accessing data.
 * When the previous mode is USR or SYS acquire the kernel mutex
 * Only other time is in SVC mode with kernel lock already acquired by
 * this process AND the try/catch pc/sp is initialized.
 * Other modes and times it is a kernel panic.
 */

void data_abort_handler(struct UserContext *context)
{
  bits32_t dfsr;
  bits32_t access;
  vm_addr fault_addr;
  uint32_t mode;
  uint32_t status;
  
  struct Process *current;
  
//  Info ("data_abort_handler");
  
  current = get_current_process();

  dfsr = hal_get_dfsr();
  fault_addr = hal_get_far();

//	Info("CPSR=%08x", (uint32_t)context->cpsr);
//	Info("SCTLR=%08x", (uint32_t)hal_get_sctlr());
	
  // FIXME: Must not enable interrupts before getting above registers?

#if 1
	if (fault_addr >= 0x0001C000 && fault_addr <= 0x00028000) {
		Info("DFSR=%08x", (uint32_t)dfsr);

		status = DFSR_STATUS(dfsr);

		Info("dfsr status: %d", status);

		if (status == DFSR_ALIGNMENT_FAULT) {
			Info("alignment fault");
		} else if (status == DFSR_PERMISSION_FAULT) {
			Info("permission fault");
		}
	}
#endif

  mode = context->cpsr & CPSR_MODE_MASK;
  if (mode == USR_MODE || mode == SYS_MODE) {
    KernelLock();
  } else if (bkl_owner != current) {
    DisableInterrupts();
    PrintUserContext(context);
//    Error("fault addr = %08x", fault_addr);
    KernelPanic();
  }
  
  if (dfsr & DFSR_RW)
    access = PROT_WRITE;
  else
    access = PROT_READ;

  if (page_fault(fault_addr, access) != 0) {
    if (mode == SVC_MODE && current->catch_state.pc != 0xdeadbeef) {
      Error("Page fault failed during copyin/copyout");
      Error("fault_addr: %08x, access:%08x", fault_addr, access);
      
      context->pc = current->catch_state.pc;
      current->catch_state.pc = 0xdeadbeef;
    } else if (mode == USR_MODE || mode == SYS_MODE) {
      PrintUserContext(context);
      Error("Unhandled USER Data Abort: fault addr = %08x", fault_addr);
      Error("Stack:");
      PrintMemDump(context->sp, 32);
      Error("PC:");
      PrintMemDump(context->pc, 32);
      Error("mode = %08x", mode);
      
      KernelPanic();  // FIXME: Remove

/*      
      current->signal.si_code[SIGSEGV-1] = 0;    // TODO: Could be access bit?
      current->signal.sigsegv_ptr = fault_addr;
      sys_kill(current->pid, SIGSEGV);
*/            
    } else {
      PrintUserContext(context);
      Error("fault addr = %08x", fault_addr);
      KernelPanic();
    }
  }

  KASSERT(context->pc != 0);

  if (mode == USR_MODE || mode == SYS_MODE) {
    CheckSignals(context);

    if (bkl_locked == false) {
      DisableInterrupts();
      PrintUserContext(context);
      KernelPanic();
    }

    KernelUnlock();
  }
}


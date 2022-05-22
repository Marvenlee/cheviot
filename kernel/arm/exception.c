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

#define KDEBUG

#include <kernel/arm/arm.h>
#include <kernel/arm/globals.h>
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



void PrintUserContext(struct UserContext *uc) {
  Info("pc = %08x,   sp = %08x", uc->pc, uc->sp);
  Info("lr = %08x, cpsr = %08x", uc->lr, uc->cpsr);
  Info("r0 = %08x,   r1 = %08x", uc->r0, uc->r1);
  Info("r2 = %08x,   r3 = %08x", uc->r2, uc->r3);
  Info("r4 = %08x,   r5 = %08x", uc->r4, uc->r5);
  Info("r6 = %08x,   r7 = %08x", uc->r6, uc->r7);
  Info("r8 = %08x,   r9 = %08x", uc->r8, uc->r9);
  Info("r10 = %08x,  r11 = %08x   r12 = %08x", uc->r10, uc->r11, uc->r12);
}

/*
 *
 */

void ResetHandler(void) {
  KernelPanic();
}

/*
 *
 */

void ReservedHandler(void) {
  KernelPanic();
}

/*
 *
 */

void FiqHandler(void) {
  KernelPanic();
}

/*
 *
 */

void SysUnknownSyscallHandler(struct UserContext *context) {
  struct Process *current;
  
  current = GetCurrentProcess();
  current->signal.si_code[SIGSYS-1] = 0;
  SysKill(current->pid, SIGSYS);
}

/*
 *
 */

void UndefInstrHandler(struct UserContext *context) {
  struct Process *current;
  
  current = GetCurrentProcess();

  if ((context->cpsr & CPSR_MODE_MASK) != USR_MODE &&
      (context->cpsr & CPSR_MODE_MASK) != SYS_MODE) {
    KernelPanic();
  }

  current->signal.si_code[SIGILL-1] = 0;
  current->signal.sigill_ptr = context->pc;
  SysKill(current->pid, SIGILL);
}

/*
 *
 */

void PrefetchAbortHandler(struct UserContext *context) {
  vm_addr fault_addr;
  uint32_t mode;
  struct Process *current;
  
  current = GetCurrentProcess();

  fault_addr = context->pc;

  mode = context->cpsr & CPSR_MODE_MASK;
  if (mode == USR_MODE || mode == SYS_MODE) {
    KernelLock();
  } else if (bkl_owner != current) {
    DisableInterrupts();
    PrintUserContext(context);
    Info("Prefetch Abort bkl not owner, fault addr = %08x", fault_addr);
    KernelPanic();
  } else {
    DisableInterrupts();
    PrintUserContext(context);
    Info("Prefetch Abort in kernel, fault addr = %08x", fault_addr);
    KernelPanic();
  }

  if (PageFault(fault_addr, PROT_EXEC) != 0) {
    if (mode == USR_MODE || mode == SYS_MODE) {
      PrintUserContext(context);
      Info("fault addr = %08x", fault_addr);

      current->signal.si_code[SIGSEGV-1] = 0;    // TODO: Could be access bit?
      current->signal.sigsegv_ptr = fault_addr;
      SysKill(current->pid, SIGSEGV);
      
    } else {
      PrintUserContext(context);
      Info("fault addr = %08x", fault_addr);
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

/*
 * Page fault when accessing data.
 * When the previous mode is USR or SYS acquire the kernel mutex
 * Only other time is in SVC mode with kernel lock already acquired by
 * this process AND the try/catch pc/sp is initialized.
 * Other modes and times it is a kernel panic.
 */

void DataAbortHandler(struct UserContext *context) {
  bits32_t dfsr;
  bits32_t access;
  vm_addr fault_addr;
  uint32_t mode;
  struct Process *current;
  
  current = GetCurrentProcess();

  dfsr = GetDFSR();
  fault_addr = GetFAR();

  // FIXME: Must not enable interrupts before getting above registers?

  mode = context->cpsr & CPSR_MODE_MASK;
  if (mode == USR_MODE || mode == SYS_MODE) {
    KernelLock();
  } else if (bkl_owner != current) {
    DisableInterrupts();
    PrintUserContext(context);
    Error("fault addr = %08x", fault_addr);
    KernelPanic();
  }
  
  if (dfsr & DFSR_RW)
    access = PROT_WRITE;
  else
    access = PROT_READ;

  if (PageFault(fault_addr, access) != 0) {
    if (mode == SVC_MODE && current->catch_state.pc != 0xdeadbeef) {
      Error("Page fault failed during copyin/copyout");
      context->pc = current->catch_state.pc;
      current->catch_state.pc = 0xdeadbeef;
    } else if (mode == USR_MODE || mode == SYS_MODE) {
      PrintUserContext(context);
      Error("? USER Data Abort: fault addr = %08x", fault_addr);

      current->signal.si_code[SIGSEGV-1] = 0;    // TODO: Could be access bit?
      current->signal.sigsegv_ptr = fault_addr;
      SysKill(current->pid, SIGSEGV);
            
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


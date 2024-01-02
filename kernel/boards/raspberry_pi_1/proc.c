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
 * ARM-specific process creation and deletion code.
 */

#include <kernel/board/arm.h>
#include <kernel/board/globals.h>
#include <kernel/board/init.h>
#include <kernel/board/task.h>
#include <kernel/dbg.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <string.h>

extern void StartForkProcess(void);
extern void StartExecProcess(void);

/* @brief   Set CPU registers in preparation for a process forking
 *
 * Executed on the context of the parent process. Initializes CPU state
 * and kernel stack so that PrepareProcess() is executed on the context
 * of the new process.
 */
int arch_fork_process(struct Process *proc, struct Process *current)
{
  struct UserContext *uc_current;
  struct UserContext *uc_proc;
  uint32_t *context;

  uc_current = (struct UserContext *)((vm_addr)current + PROCESS_SZ -
                                      sizeof(struct UserContext));
  uc_proc = (struct UserContext *)((vm_addr)proc + PROCESS_SZ -
                                   sizeof(struct UserContext));

  uc_proc->r0 = 0;
  uc_proc->r1 = uc_current->r1;
  uc_proc->r2 = uc_current->r2;
  uc_proc->r3 = uc_current->r3;
  uc_proc->r4 = uc_current->r4;
  uc_proc->r5 = uc_current->r5;
  uc_proc->r6 = uc_current->r6;
  uc_proc->r7 = uc_current->r7;
  uc_proc->r8 = uc_current->r8;
  uc_proc->r9 = uc_current->r9;
  uc_proc->r10 = uc_current->r10;
  uc_proc->r11 = uc_current->r11;
  uc_proc->r12 = uc_current->r12;
  uc_proc->sp = uc_current->sp;
  uc_proc->lr = uc_current->lr;
  uc_proc->pc = uc_current->pc;
  uc_proc->cpsr = uc_current->cpsr;

  // Simple question, Is uc_current pointing to actual context of root process,
  // or not?

  context = ((uint32_t *)uc_proc) - 15;

  KASSERT(uc_proc->pc != 0);

  for (int t = 0; t < 13; t++) {
    context[t] = 0xdeadbee0 + t;
  }

  context[13] = (uint32_t)uc_proc;
  context[14] = (uint32_t)StartForkProcess;

  proc->context = context;
  proc->cpu = current->cpu;

  proc->catch_state.pc = current->catch_state.pc;

  return 0;
}


/* @brief   Set CPU registers in preparation for a process Exec'ing
 */
void arch_init_exec(struct Process *proc, void *entry_point,
                  void *stack_pointer, struct execargs *args)
{
  struct UserContext *uc;
  uint32_t *context;
  uint32_t cpsr;

  cpsr = cpsr_dnm_state | USR_MODE | CPSR_DEFAULT_BITS; 
  uc = (struct UserContext *)((vm_addr)proc + PROCESS_SZ -
                              sizeof(struct UserContext));

  memset(uc, 0, sizeof(*uc));

  uc->r0 = args->argc;
  uc->r1 = (uint32_t)args->argv;
  uc->r2 = args->envc;
  uc->r3 = (uint32_t)args->envv;
  uc->pc = (uint32_t)entry_point;
  uc->sp = (uint32_t)stack_pointer;
  uc->cpsr = cpsr;

  context = ((uint32_t *)uc) - 15;

  for (int t = 0; t < 13; t++) {
    context[t] = 0;
  }

  context[13] = (uint32_t)uc;
  context[14] = (uint32_t)StartExecProcess;
  proc->context = context;

  proc->catch_state.pc = 0xdeadbeef;

  GetContext(proc->context);
}


/* @brief   Architecture specific handling when freeing a process
 */
void arch_free_process(struct Process *proc)
{
}


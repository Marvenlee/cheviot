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
#include <kernel/dbg.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>

/*
 * Interrupts aren't nested, all have equal priority.
 * Also, how do we preempt the kernel on arm/use separate stacks?
 *
 * There are 64 videocore IRQa and about 8 ARM IRQs.
 *
 * TODO: Can we set the vector table to 0xFFFF0000 by flag in cr15 ?
 * NOTE: When setting the vector table entries at address 0 an exception
 * occurred.
 * Yet addresses a few bytes above 0 worked fine. Need to look into it.
 */
void InitArm(void) {
  vm_addr vbar;

  cpsr_dnm_state = GetCPSR() & CPSR_DNM_MASK;

  vbar = (vm_addr)vector_table;

  *(uint32_t volatile *)(vbar + 0x00) = (LDR_PC_PC | 0x18);
  *(uint32_t volatile *)(vbar + 0x04) = (LDR_PC_PC | 0x18);
  *(uint32_t volatile *)(vbar + 0x08) = (LDR_PC_PC | 0x18);
  *(uint32_t volatile *)(vbar + 0x0C) = (LDR_PC_PC | 0x18);
  *(uint32_t volatile *)(vbar + 0x10) = (LDR_PC_PC | 0x18);
  *(uint32_t volatile *)(vbar + 0x14) = (LDR_PC_PC | 0x18);
  *(uint32_t volatile *)(vbar + 0x18) = (LDR_PC_PC | 0x18);
  *(uint32_t volatile *)(vbar + 0x1C) = (LDR_PC_PC | 0x18);

  /* setup the secondary vector table in RAM */
  *(uint32_t volatile *)(vbar + 0x20) = (uint32_t)reset_vector;
  *(uint32_t volatile *)(vbar + 0x24) = (uint32_t)undef_instr_vector;
  *(uint32_t volatile *)(vbar + 0x28) = (uint32_t)swi_vector;
  *(uint32_t volatile *)(vbar + 0x2C) = (uint32_t)prefetch_abort_vector;
  *(uint32_t volatile *)(vbar + 0x30) = (uint32_t)data_abort_vector;
  *(uint32_t volatile *)(vbar + 0x34) = (uint32_t)reserved_vector;
  *(uint32_t volatile *)(vbar + 0x38) = (uint32_t)irq_vector;
  *(uint32_t volatile *)(vbar + 0x3C) = (uint32_t)fiq_vector;

  SetVBAR((vm_addr)vector_table);

  uint32_t ctrl = GetCtrl();
  ctrl |= C1_XP;
  SetCtrl(ctrl);

  // TODO: What is DACR, why set to 0x55555555 ?
  uint32_t dacr;

  dacr = GetDACR();
  SetDACR(0x55555555);
  dacr = GetDACR();

  EnableL1Cache();

  // TODO SPSR, does this contain the interrupt

  InitTimer();
}

/*
*/
void InitTimer(void) {
  uint32_t clo;

  clo = timer_regs->clo;
  clo += MICROSECONDS_PER_JIFFY;
  timer_regs->c3 = clo;

  EnableIRQ(INTERRUPT_TIMER3);
}

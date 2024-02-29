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

/*
 * Kernel initialization.
 */

#include <kernel/arch.h>
#include <kernel/board/boot.h>
#include <kernel/board/globals.h>
#include <kernel/board/init.h>
#include <kernel/dbg.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>

/* @brief Initialize processor-specific tables, peripherals and registers
 *
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
void init_arm(void)
{
  vm_addr vbar;

  Info("init_arm()");
  
  cpsr_dnm_state = hal_get_cpsr() & CPSR_DNM_MASK;

  Info(".. cpsr = %08x", (uint32_t)cpsr_dnm_state);
    
  Info(".. setting vector_table pointers");

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

  Info("..hal_set_vbar(%08x)", (uint32_t)vector_table);
  
  hal_set_vbar((vm_addr)vector_table);
}


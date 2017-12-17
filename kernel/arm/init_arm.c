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

#include <kernel/types.h>
#include <kernel/vm.h>
#include <kernel/proc.h>
#include <kernel/dbg.h>
#include <kernel/utility.h>
#include <kernel/arm/init.h>
#include <kernel/arch.h>
#include <kernel/globals.h>
#include <kernel/arm/boot.h>


/*
 * Interrupts aren't nested, all have equal priority.
 * Also, how do we preempt the kernel on arm/use separate stacks?
 *
 * There are 64 videocore IRQa and about 8 ARM IRQs.
 *
 * TODO: Can we set the vector table to 0xFFFF0000 by flag in cr15 ?
 * NOTE: When setting the vector table entries at address 0 an exception occurred.
 * Yet addresses a few bytes above 0 worked fine. Need to look into it.
 */
void InitArm (void)
{
    KLog  ("InitArm()");
     
    cpsr_dnm_state = GetCPSR() & CPSR_DNM_MASK;

    KLog  ("InitArm() - set primary vector table");
 
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x80000000 + 0x00) = (LDR_PC_PC | 0x18);
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x80000000 + 0x04) = (LDR_PC_PC | 0x18);
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x80000000 + 0x08) = (LDR_PC_PC | 0x18);
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x80000000 + 0x0C) = (LDR_PC_PC | 0x18);
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x80000000 + 0x10) = (LDR_PC_PC | 0x18);
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x80000000 + 0x14) = (LDR_PC_PC | 0x18);
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x80000000 + 0x18) = (LDR_PC_PC | 0x18);
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x80000000 + 0x1C) = (LDR_PC_PC | 0x18);
        
    /* setup the secondary vector table in RAM */
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x80000000 + 0x20) = (uint32)reset_vector;
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x80000000 + 0x24) = (uint32)undef_instr_vector;
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x80000000 + 0x28) = (uint32)swi_vector;
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x80000000 + 0x2C) = (uint32)prefetch_abort_vector;
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x80000000 + 0x30) = (uint32)data_abort_vector;
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x80000000 + 0x34) = (uint32)reserved_vector;
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x80000000 + 0x38) = (uint32)irq_vector;
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x80000000 + 0x3C) = (uint32)fiq_vector;

    InitTimer();
}


/*
*/ 
void InitTimer (void)
{
    uint32 clo;

    clo = timer_regs->clo;
    clo += MICROSECONDS_PER_JIFFY;
    timer_regs->c3 = clo;
    EnableIRQ (INTERRUPT_TIMER3);
}


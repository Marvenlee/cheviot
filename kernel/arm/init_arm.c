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
 * ARM and Raspberry Pi initialization.
 */

#include <kernel/types.h>
#include <kernel/vm.h>
#include <kernel/proc.h>
#include <kernel/dbg.h>
#include <kernel/arch.h>
#include <kernel/globals.h>



/*
 * Interrupts aren't nested, all have equal priority.
 * Also, how do we preempt the kernel on arm/use separate stacks?
 *
 * There are 64 videocore IRQa and about 8 ARM IRQs.
 */


void InitArm (void)
{
    KLOG ("InitArm() ***");

    cpsr_dnm_state = GetCPSR() & CPSR_DNM_MASK;
    
    

    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x00) = (LDR_PC_PC | 0x18);
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x04) = (LDR_PC_PC | 0x18);
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x08) = (LDR_PC_PC | 0x18);
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x0C) = (LDR_PC_PC | 0x18);
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x10) = (LDR_PC_PC | 0x18);
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x14) = (LDR_PC_PC | 0x18);;
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x18) = (LDR_PC_PC | 0x18);
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x1C) = (LDR_PC_PC | 0x18);
    
    /* setup the secondary vector table in RAM */
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x20) = (uint32)reset_vector;
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x24) = (uint32)undef_instr_vector;
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x28) = (uint32)swi_vector;
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x2C) = (uint32)prefetch_abort_vector;
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x30) = (uint32)data_abort_vector;
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x34) = (uint32)reserved_vector;
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x38) = (uint32)irq_vector;
    *(uint32 volatile *)(VECTOR_TABLE_ADDR + 0x3C) = (uint32)fiq_vector;


    // AWOOGA (We're still using physical addresses of peripheral base here !!!!!!!!!!!!!
    // 
    //
    //
    // Have we just mapped the screen buffer or the entire peripherial IO area
    // at top of address space?
    
    timer_regs = (struct bcm2835_system_timer_registers *)
                            ((uint8 *)ST_BASE + peripheral_base - peripheral_phys);

    interrupt_regs = (struct bcm2835_interrupt_registers *)
                            ((uint8 *)ARMCTRL_IC_BASE + peripheral_base - peripheral_phys);
    

    InitTimer();
    
}




/*
 *
 */
 
void InitTimer (void)
{
    uint32 clo;
    
    clo = timer_regs->clo;
    clo += MICROSECONDS_PER_JIFFY;
    timer_regs->c3 = clo;

    EnableIRQ (INTERRUPT_TIMER3);
}



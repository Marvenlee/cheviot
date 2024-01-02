/*
 * Copyright 2023  Marven Gilhespie
 *
 * Licensed under the Apache License, segment_id 2.0 (the "License");
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

#ifndef MACHINE_BOARD_HAL_INTERRUPT_H
#define MACHINE_BOARD_HAL_INTERRUPT_H

#include <stdint.h>
#include <stdbool.h>


/*
 * Interupt controller
 */
struct bcm2835_interrupt_registers
{
  uint32_t irq_basic_pending;
  uint32_t irq_pending_1;
  uint32_t irq_pending_2;
  uint32_t fiq_control;
  uint32_t enable_irqs_1;
  uint32_t enable_irqs_2;
  uint32_t enable_basic_irqs;
  uint32_t disable_irqs_1;
  uint32_t disable_irqs_2;
  uint32_t disable_basic_irqs;
};


/*
 * Interrupt assignments
 */
#define ARM_IRQ1_BASE 0
#define INTERRUPT_TIMER0 (ARM_IRQ1_BASE + 0)
#define INTERRUPT_TIMER1 (ARM_IRQ1_BASE + 1)
#define INTERRUPT_TIMER2 (ARM_IRQ1_BASE + 2)
#define INTERRUPT_TIMER3 (ARM_IRQ1_BASE + 3)

#define NIRQ (32 + 32 + 20)


/*
 * Prototypes
 */
void hal_set_intc_base(void *base_addr);
void *hal_get_intc_base(void);


#endif



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
#include <kernel/board/boot.h>
#include <kernel/board/globals.h>
#include <kernel/board/init.h>
#include <kernel/board/timer.h>
#include <kernel/dbg.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>


/* @brief Initialize the timer peripheral
*/
void init_timer_registers(void)
{
  uint32_t clo;

  clo = hal_mmio_read(&timer_regs->clo);
  clo += MICROSECONDS_PER_JIFFY;
  hal_mmio_write(&timer_regs->c3, clo);

  enable_irq(INTERRUPT_TIMER3);
}


/* @brief   Special-case handling of timer interrupt
 *
 */
void interrupt_top_half_timer(void)
{
  uint32_t clo;
  uint32_t status;

  status = hal_mmio_read(&timer_regs->cs);

  if (status & ST_CS_M3) {
    clo = hal_mmio_read(&timer_regs->clo);
    clo += MICROSECONDS_PER_JIFFY;
    hal_mmio_write(&timer_regs->c3, clo);    
    hal_mmio_write(&timer_regs->cs, ST_CS_M3);

    TimerTopHalf();
  }
}


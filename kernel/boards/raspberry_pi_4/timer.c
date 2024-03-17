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

#include <kernel/arch.h>
#include <kernel/board/boot.h>
#include <kernel/board/globals.h>
#include <kernel/board/init.h>
#include <kernel/board/timer.h>
#include <kernel/board/interrupt.h>
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

  Info("init_timer_registers");

  clo = hal_mmio_read(&timer_regs->clo);
  clo += MICROSECONDS_PER_JIFFY;
  hal_mmio_write(&timer_regs->c3, clo);

#if 0
  set_irq_affinity(IRQ_TIMER3, 0);
  set_irq_config(IRQ_TIMER3, IRQ_CFG_LEVEL);
  set_irq_priority(IRQ_TIMER3, GIC_PRI_IRQ);
#endif

  // Enable the system timer interrupt
  enable_irq(IRQ_TIMER3);
}


/* @brief   Special-case handling of timer interrupt
 *
 * TODO: We may want to support sub-jiffy timer interrupts and/or go semi-tickless,
 * to avoid waking up after every jiffy.
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


/* @brief		Read the 64-bit free running microsecond system timer
 *
 */
uint64_t timer_read(void)
{
	volatile uint32_t chi1, chi2, clo;
	
  do {
	  chi1 = hal_mmio_read(&timer_regs->chi);    
	  clo = hal_mmio_read(&timer_regs->clo);    
	  chi2 = hal_mmio_read(&timer_regs->chi);    			
	} while (chi1 != chi2);
	
	return (uint64_t)chi2 << 32ULL | (uint64_t)clo; 
}


/* @brief		gettime handling of architecture-specific timer sources
 *
 * CLOCK_MONOTONIC_RAW:  This uses the microsecond, 64-bit free-running counter.
 *                       There are approximately 2^44 seconds before the counter wraps around.
 */
int arch_clock_gettime(int clock_id, struct timespec *ts)
{
	int sc = 0;
	uint64_t clock;
	
	Info("arch_clock_gettime");
	
	switch (clock_id) {
		case CLOCK_MONOTONIC_RAW:
			clock = timer_read();
			ts->tv_sec = clock / 1000000ULL;
			ts->tv_nsec = (clock * 1000) % 1000000000ULL;
						
			break;
		default:
			Error("arch_clock_gettime() -EINVAL");
			sc = -EINVAL;
	}
	
	return sc;
}


/* @brief		Handle nanosleep for sub-jiffy delays
 *
 * We use a spin-loop here to wait for short delays by repeatedly reading
 * the microsecond system timer.
 *
 * Alternatively we could have a small pool of timers to support sub-jiffy delays
 * using interrupts. If there are too many short delays then put them onto the
 * normal timing wheel.
 */
int arch_spin_nanosleep(struct timespec *req)
{
	uint64_t start_clock;
	uint64_t current_clock;
	uint64_t timeout = req->tv_nsec / 1000;
	
	start_clock = timer_read();
	
	do
	{
		current_clock = timer_read();
	}	while (current_clock - start_clock < timeout);
		
	return 0;
}



/*
 * Copyright 2023  Marven Gilhespie
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

#include <machine/cheviot_hal.h>
#include "gpio.h"


// BCM2835 GPIO registers
struct bcm2835_gpio_registers *gpio_regs;

/*
 *
 */
static void io_delay(uint32_t cycles) 
{
  while(cycles-- > 0) {
    hal_isb();
  }
}


/*
 *
 */
void configure_gpio(uint32_t pin, enum FSel fn, enum PullUpDown action)
{    
  hal_dsb();
  
  // set pull up down configuration and delay for 150 cycles.
  hal_mmio_write(&gpio_regs->pud, (uint32_t)action);
  io_delay(10);

  // trigger action and delay for 150 cycles.
  hal_mmio_write (&gpio_regs->pud_clk[pin / 32], 1 << (pin % 32));
  io_delay(10);
  
  // clear action
  hal_mmio_write(&gpio_regs->pud, (uint32_t)PULL_NONE);

  // remove clock
  hal_mmio_write (&gpio_regs->pud_clk[pin / 32], 0);

  // set function
  // ------------
  uint32_t fsel = hal_mmio_read(&gpio_regs->fsel[pin / 10]);
  uint32_t shift = (pin % 10) * 3;
  uint32_t mask = ~(7U << shift);  
  fsel = (fsel & mask) | (fn << shift);  
  hal_mmio_write(&gpio_regs->fsel[pin / 10], fsel);
}


/* @brief   Set or clear output of pin
 */
void set_gpio(uint32_t pin, bool state)
{
  if (state) {
    hal_mmio_write(&gpio_regs->set[pin / 32], 1U << (pin % 32));
  } else {
    hal_mmio_write(&gpio_regs->clr[pin / 32], 1U << (pin % 32));
  }
}


/*
 *
 */
bool get_gpio(uint32_t pin)
{  
  uint32_t lev = hal_mmio_read(&gpio_regs->lev[pin / 32]);

  if (lev & (1U << (pin % 32)) != 0) {
    return true;
  } else {
    return false;
  }
}



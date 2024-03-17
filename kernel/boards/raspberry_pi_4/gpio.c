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
#include <kernel/arch.h>
#include <kernel/board/boot.h>
#include <kernel/board/globals.h>
#include <kernel/board/gpio.h>
#include <kernel/dbg.h>
#include <kernel/globals.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>



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
int sys_rpi_configure_gpio(uint32_t pin, enum FSel fn, enum PullUpDown action)
{    
  hal_dsb();
  
	if (pin > MAX_GPIO_PIN || fn >= MAX_FSEL || action >= MAX_PUPDN) {
		return -EINVAL;
	}

  // set pull up down configuration.
	//  io_delay(10);
  hal_mmio_write (&gpio_regs->pup_pdn_cntrl[pin/16], (action & 0x03) << (pin%16));
	//  io_delay(10);
  
  // set function
  uint32_t fsel = hal_mmio_read(&gpio_regs->fsel[pin / 10]);
  uint32_t shift = (pin % 10) * 3;
  uint32_t mask = ~(7U << shift);  
  fsel = (fsel & mask) | (fn << shift);    
  hal_mmio_write(&gpio_regs->fsel[pin / 10], fsel);
  return 0;
}


/* @brief   Set or clear output of pin
 */
int sys_rpi_set_gpio(uint32_t pin, bool state)
{
	if (pin > MAX_GPIO_PIN) {
		return -EINVAL;
	}

  if (state) {
    hal_mmio_write(&gpio_regs->set[pin / 32], 1U << (pin % 32));
  } else {
    hal_mmio_write(&gpio_regs->clr[pin / 32], 1U << (pin % 32));
  }
  
  return 0;
}


/*
 *
 */
int sys_rpi_get_gpio(uint32_t pin)
{
	if (pin > MAX_GPIO_PIN) {
		return -EINVAL;
	}

  uint32_t lev = hal_mmio_read(&gpio_regs->lev[pin / 32]);

  if (lev & (1U << (pin % 32)) != 0) {
    return 1;
  } else {
    return 0;
  }
}







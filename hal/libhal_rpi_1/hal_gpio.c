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

#include <machine/raspberry_pi_hal.h>


// BCM2835 GPIO registers
static struct bcm2835_gpio_registers *gpio_regs;


/*
 *
 */
void hal_set_gpio_base(void *base_addr)
{
  gpio_regs = base_addr;
}


/*
 *
 */
void *hal_get_gpio_base(void)
{
  return gpio_regs;
}


/*
 *
 * NOTE: This requires access to the hal_timer
 */
void hal_configure_gpio(uint32_t pin, enum FSel fn, enum PullUpDown action)
{    
  hal_memory_barrier();
  
  // set pull up down configuration and delay for 150 cycles.
  volatile uint32_t *pud = &gpio_regs->pud;
  *pud = action;
  hal_delay_microsecs(10);

  // trigger action and delay for 150 cycles.
  volatile uint32_t *clock = &gpio_regs->pud_clk[pin / 32];
  *clock = (1 << (pin % 32));
  hal_delay_microsecs(10);
  
  // clear action
  *pud = OFF;

  // remove clock
  *clock = 0;

  // set function
  // ------------
  volatile uint32_t *fsel = &gpio_regs->fsel[pin / 10];
  uint32_t shift = (pin % 10) * 3;
  uint32_t mask = ~(7U << shift);
  *fsel = (*fsel & mask) | (fn << shift);
  
  hal_memory_barrier();
}


/* @brief   Set or clear output of pin
 */
void hal_set_gpio(uint32_t pin, bool state)
{
  hal_memory_barrier();
  
  if (state) {
    gpio_regs->set[pin / 32] = 1U << (pin % 32);
  } else {
    gpio_regs->clr[pin / 32] = 1U << (pin % 32);
  }

  hal_memory_barrier();
}


/*
 *
 */
bool hal_get_gpio(uint32_t pin)
{
  
}


/*
 */
void hal_led_on(void)
{
  hal_memory_barrier();

  gpio_regs->fsel[1] = (1 << 18);
  gpio_regs->clr[0] = (1 << 16);

  hal_memory_barrier();
}


/*
 */
void hal_led_off(void)
{
  gpio_regs->fsel[1] = (1 << 18);
  gpio_regs->set[0] = (1 << 16);
}


/*
 */
void hal_blink_led(int cnt)
{
  int t;

  for (t = 0; t < cnt; t++) {
    LedOn();
    hal_delay_microsecs(200000);

    LedOff();
    hal_delay_microsecs(200000);
  }
}


/* @brief   Blink LEDs continuously to indicate error
 */
void hal_blink_error(void)
{
  while (1) {
    LedOn();
    hal_delay_microsecs(100000);

    LedOff();
    hal_delay_microsecs(100000);
  }
}


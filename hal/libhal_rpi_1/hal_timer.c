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


// BCM2835 System Timer registers
static struct bcm2835_timer_regs *timer_regs;


/*
 *
 */
void hal_set_timer_base(void *base_addr)
{
  timer_regs = base_addr;
}


/*
 *
 */
void *hal_get_timer_base(void)
{
  return timer_regs;
}


/*
 *
 */
uint32_t hal_read_timer(void)
{
  return mmio_read(&timer_regs->clo);
}


/*
 *
 */
void hal_delay_microsecs(uint32_t usec)
{
  uint32_t start = hal_read_timer();
  
  do {
    uint32_t now = hal_read_timer();
  } while ((now - start) < usec);
}


/*
 *
 */
uint32_t hal_compare_timer(uint32_t start_usec)
{
  uint32_t now = hal_read_timer();  
  return now - start_usec;
}



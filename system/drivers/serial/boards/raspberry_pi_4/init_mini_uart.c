/*
 * Copyright 2019  Marven Gilhespie
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

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "serial.h"
#include "sys/debug.h"
#include <sys/syscalls.h>
#include <sys/event.h>


#define AUX_MU_BAUD(baud) ((AUX_UART_CLOCK/(baud*8))-1)

/*
 *
 */ 
void configure_uart(void)
{
  aux = virtualallocphys((void *)0x50000000, 4096,
                          PROT_READ | PROT_WRITE | CACHE_UNCACHEABLE,
                          (void *)(BCM2835_PERI_BASE + UART_BASE_PA));  
  if (aux == NULL) {
    return -ENOMEM;
  }  
  
  gpio = virtualallocphys((void *)0x50010000, 4096, 
                          PROT_READ | PROT_WRITE | CACHE_UNCACHEABLE,
                          (void *)(BCM2835_PERI_BASE + GPIO_BASE_PA));  
  if (gpio == NULL) {
    return -ENOMEM;
  }

  dmb();
  
  aux->enables = 1;
  aux->mu_ier_reg = 0;
  aux->mu_cntl_reg = 0;
  aux->mu_lcr_reg = 3;
  aux->mu_mcr_reg = 0;
  aux->mu_ier_reg = 0;
  aux->mu_iir_reg = 0xC6;
  aux->mu_baud_reg = AUX_MU_BAUD(115200)); 

  dmb();

  // select function 0 and disable pull up/down for pins 14, 15
//  configure_gpio(14, FN0, OFF);
//  configure_gpio(15, FN0, OFF);

  gpio_useAsAlt5(14);
  gpio_useAsAlt5(15);

  dmb();

  aux->mu_cntl_reg = 3; //enable RX/TX

  dmb();
  
#if USE_INTERRUPTS
  interrupt_fd = createinterrupt(SERIAL_IRQ, &interrupt_handler);

  if (interrupt_fd < 0) {
    return -ENOMEM;
  }
#endif
  return 0;
}


/*
 *
 */
void configure_gpio(uint32_t pin, enum FSel fn, enum PullUpDown action)
{
    dmb();
    
    // set pull up down
    // ----------------
    
    // set action & delay for 150 cycles.
    volatile uint32_t *pud = &gpio->pud;
    *pud = action;
    io_delay(1000);

    // trigger action & delay for 150 cycles.
    volatile uint32_t *clock = &gpio->pud_clk[pin / 32];
    *clock = (1 << (pin % 32));
    io_delay(1000);
    
    // clear action
    *pud = OFF;
	
    // remove clock
    *clock = 0;

    // set function
    // ------------
    volatile uint32_t *fsel = &gpio->fsel[pin / 10];
    uint32_t shift = (pin % 10) * 3;
    uint32_t mask = ~(7U << shift);
    *fsel = (*fsel & mask) | (fn << shift);
    
    dmb();
}

/*
 *
 */
void set_gpio(uint32_t pin, bool state)
{
    if (state) {
      gpio->set[pin / 32] = 1U << (pin % 32);
    } else {
      gpio->clr[pin / 32] = 1U << (pin % 32);
    }
}


/*
 *
 */
void io_delay(uint32_t cycles) 
{
    while(cycles-- > 0) isb();
}










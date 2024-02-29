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

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscalls.h>
#include <unistd.h>
#include <sys/debug.h>
#include <sys/lists.h>
#include <machine/cheviot_hal.h>
#include "peripheral_base.h"
#include "aux_uart.h"
#include "gpio.h"
#include "common.h"
#include "globals.h"


/*
 * Memory mapped IO locations
 */

struct bcm2711_aux_registers *aux_regs;
//struct bcm2711_gpio_registers *gpio_regs;


/*
 *
 */ 
int aux_uart_configure(int baud)
{
  aux_regs = virtualallocphys((void *)0x50000000, 4096,
                          PROT_READ | PROT_WRITE | CACHE_UNCACHEABLE,
                          (void *)(AUX_BASE));  
  if (aux_regs == NULL) {
    return -ENOMEM;
  }  
  
  /*
  gpio_regs = virtualallocphys((void *)0x50010000, 4096, 
                          PROT_READ | PROT_WRITE | CACHE_UNCACHEABLE,
                          (void *)(GPIO_BASE));  
  if (gpio_regs == NULL) {
    return -ENOMEM;
  }
  */
  
  log_info("aux_regs  = %08x", (uint32_t)aux_regs);
  // log_info("gpio_regs = %08x", (uint32_t)gpio_regs);

  hal_mmio_write(&aux_regs->enables, 1);
  hal_mmio_write(&aux_regs->mu_ier_reg, 0);
  hal_mmio_write(&aux_regs->mu_cntl_reg, 0);
  hal_mmio_write(&aux_regs->mu_lcr_reg, 3);
  hal_mmio_write(&aux_regs->mu_mcr_reg, 0);
  hal_mmio_write(&aux_regs->mu_ier_reg, 0);
  hal_mmio_write(&aux_regs->mu_iir_reg, 0xC6);
  hal_mmio_write(&aux_regs->mu_baud_reg, AUX_MU_BAUD(baud)); 

  // GPIO pins 14 and 15 must be configured as alternate function FN5 for Aux UART 
  // configure_gpio(14, AUX_UART_GPIO_ALT_FN, PULL_NONE);
  // configure_gpio(15, AUX_UART_GPIO_ALT_FN, PULL_NONE);


#ifdef USE_INTERRUPTS
  // TODO: Configure interrupts

  interrupt_fd = createinterrupt(AUX_UART_IRQ, &interrupt_handler);

  if (interrupt_fd < 0) {
    panic("aux: cannot create interrupt handler");
    return -ENOMEM;
  }
#endif

  hal_mmio_write(&aux_regs->mu_cntl_reg, AUX_CNTL_TXEN | AUX_CNTL_RXEN);
  return 0;
}


bool aux_uart_write_ready(void)
{ 
  return (hal_mmio_read(&aux_regs->mu_lsr_reg) & 0x20) ? true : false;  
}


bool aux_uart_read_ready(void)
{ 
  return (hal_mmio_read(&aux_regs->mu_lsr_reg) & 0x01) ? true : false;  
}

char aux_uart_read_byte(void)
{
  while (!aux_uart_read_ready());
  return hal_mmio_read(&aux_regs->mu_io_reg);
}

void aux_uart_write_byte(char ch)
{
    while (!aux_uart_write_ready()); 
    hal_mmio_write(&aux_regs->mu_io_reg, ch);
}












/* @brief   Aux UART Interrupt Service Routine
 *
 */
void aux_uart_interrupt_isr(int irq, struct InterruptAPI *api)
{    
#ifdef USE_INTERRUPTS
  api->MaskInterrupt(AUX_UART_IRQ);
  api->EventNotifyFromISR(api, NOTE_INT | (NOTE_IRQMASK & irq));  
#endif
}

/* @brief   Aux UART Bottom-Half interrupt handling
 */
void aux_uart_interrupt_bottom_half(void)
{
#ifdef USE_INTERRUPTS
  uint32_t mis;
  
  mis = hal_mmio_read(&aux_regs->mis);      

  if (mis & (INT_RXR | INT_RTR)) {
     taskwakeupall(&rx_rendez);
  }

  if (mis & INT_TXR) {
    taskwakeupall(&tx_rendez);
  }      

  hal_mmio_write(&aux_regs->rsrecr, 0);
  hal_mmio_write(&aux_regs->icr, 0xffffffff);
  unmaskinterrupt(AUX_UART_IRQ);
#else
  // Rely on a kevent timeout to call this function
  taskwakeupall(&rx_rendez);
  taskwakeupall(&tx_rendez);
#endif
}



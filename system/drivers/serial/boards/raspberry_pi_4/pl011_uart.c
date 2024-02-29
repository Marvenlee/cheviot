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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscalls.h>
#include <sys/debug.h>
#include <sys/lists.h>
#include <sys/event.h>
#include "pl011.h"
#include "globals.h"
#include <machine/cheviot_hal.h>
#include "boards/raspberry_pi_1/peripheral_base.h"
#include "boards/raspberry_pi_1/pl011_uart.h"
#include "boards/raspberry_pi_1/gpio.h"

/*
 * Memory mapped IO locations
 */
struct bcm2835_pl011_registers *pl011_regs;
struct bcm2835_gpio_registers *gpio_regs;

/*
 *
 */ 
int pl011_uart_configure(void)
{
  uint32_t tmp;
  
  pl011_regs = virtualallocphys((void *)0x50000000, 4096,
                          PROT_READ | PROT_WRITE | CACHE_UNCACHEABLE,
                          (void *)(PL011_BASE));  
  if (pl011_regs == NULL) {
    return -ENOMEM;
  }  
  
  gpio_regs = virtualallocphys((void *)0x50010000, 4096, 
                          PROT_READ | PROT_WRITE | CACHE_UNCACHEABLE,
                          (void *)(GPIO_BASE));  
  if (gpio_regs == NULL) {
    return -ENOMEM;
  }

  // wait for end of transmission
  // while(hal_mmio_read(&pl011_regs->flags) & FR_BUSY);

  // Disable UART0
  hal_mmio_write(&pl011_regs->ctrl, 0);
  io_delay(100);

  // flush transmit FIFO
  tmp = hal_mmio_read(&pl011_regs->lcrh);
  tmp &= ~LCRH_FEN;
  hal_mmio_write(&pl011_regs->lcrh, tmp);	

  // select function 0 for UART on pins 14, 15
  configure_gpio(14, PL011_UART_GPIO_ALT_FN, PULL_NONE);
  configure_gpio(15, PL011_UART_GPIO_ALT_FN, PULL_NONE);

  int divider = (UART_CLK)/(16 * config.baud);
  int temp = (((UART_CLK % (16 * config.baud)) * 8) / config.baud);
  int fraction = (temp >> 1) + (temp & 1);
  
  hal_mmio_write(&pl011_regs->ibrd, divider);
  hal_mmio_write(&pl011_regs->fbrd, fraction);

  // Enable FIFO & 8 bit data transmission (1 stop bit, no parity)
  hal_mmio_write(&pl011_regs->lcrh, LCRH_FEN | LCRH_WLEN8);
  hal_mmio_write(&pl011_regs->lcrh, LCRH_WLEN8);

  // set FIFO interrupts to fire at half full
  hal_mmio_write(&pl011_regs->ifls, IFSL_RX_1_2 | IFSL_TX_1_2);
  
  // Clear pending interrupts.
  hal_mmio_write(&pl011_regs->icr, INT_ALL);

  // Mask all interrupts.
  hal_mmio_write(&pl011_regs->imsc, INT_ALL);
  
#ifdef USE_INTERRUPTS
  interrupt_fd = createinterrupt(UART_IRQ, &interrupt_handler);

  if (interrupt_fd < 0) {
    panic("serial: cannot create interrupt handler");
    return -ENOMEM;
  }
#endif

  // Enable UART0 and enable tx and rx.
  hal_mmio_write(&pl011_regs->ctrl, CR_UARTEN | CR_TXW | CR_RXE);
  return 0;
}



/* @brief   PL011 UART Interrupt Service Routine
 *
 */
void pl011_uart_interrupt_isr(int irq, struct InterruptAPI *api)
{    
#ifdef USE_INTERRUPTS
  api->MaskInterrupt(PL011_UART_IRQ);
  api->EventNotifyFromISR(api, NOTE_INT | (NOTE_IRQMASK & irq));  
#endif
}

/* @brief   PL011 UART Bottom-Half interrupt handling
 */
void pl011_uart_interrupt_bottom_half(void)
{
#ifdef USE_INTERRUPTS
  uint32_t mis;
  
  mis = mmio_read(&aux_regs->mis);      

  if (mis & (INT_RXR | INT_RTR)) {
     taskwakeupall(&rx_rendez);
  }

  if (mis & INT_TXR) {
    taskwakeupall(&tx_rendez);
  }      

  hal_mmio_write(&aux_regs->rsrecr, 0);
  hal_mmio_write(&aux_regs->icr, 0xffffffff);
  unmaskinterrupt(PL011_UART_IRQ);
#else
  // Rely on a kevent timeout to call this function
  taskwakeupall(&rx_rendez);
  taskwakeupall(&tx_rendez);
#endif
}


/*
 *
 */
void pl011_uart_write_byte(char ch)
{
  while (hal_mmio_read(&pl011_regs->flags) & FR_BUSY) {
  }
  hal_mmio_write(&pl011_regs->data, ch);  
}


/*
 *
 */
char pl011_uart_read_byte(void)
{
  return (char)hal_mmio_read(&pl011_regs->data);  
}


/*
 *
 */
bool pl011_uart_rx_ready(void)
{
  if (hal_mmio_read(&pl011_regs->flags) & FR_RXFE) {
    return false;
  } else {
    return true;
  }
}


/*
 *
 */
bool pl011_uart_tx_ready(void)
{
  if (hal_mmio_read(&pl011_regs->flags) & FR_TXFF) {
    return false;
  } else {
    return true;
  }
}



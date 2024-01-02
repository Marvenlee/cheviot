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


static struct bcm2835_pl011_regs *pl011_regs;


/*
 *
 */
void hal_set_pl011_base(void *base_addr)
{
  pl011_regs = base_addr;
}


/*
 *
 */
void *hal_get_pl011_base(void)
{
  return pl011_regs;
}


/*
 *
 */ 
void hal_pl011_init(int baud)
{
  // wait for end of transmission
  // while(pl011_regs->flags & FR_BUSY) { }

  // Disable UART0
  pl011_regs->ctrl = 0;
  io_delay(100);

  // flush transmit FIFO
  pl011_regs->lcrh &= ~LCRH_FEN;	

  // select function 0 and disable pull up/down for pins 14, 15
  dmb();
  configure_gpio(14, FN0, OFF);
  configure_gpio(15, FN0, OFF);
  dmb();
    
  int divider = (UART_CLK)/(16 * baud);
  int temp = (((UART_CLK % (16 * baud)) * 8) / baud);
  int fraction = (temp >> 1) + (temp & 1);
  
  pl011_regs->ibrd = divider;
  pl011_regs->fbrd = fraction;

  // Enable FIFO & 8 bit data transmission (1 stop bit, no parity)
  pl011_regs->lcrh = LCRH_FEN | LCRH_WLEN8;
  pl011_regs->lcrh = LCRH_WLEN8;

  // set FIFO interrupts to fire at half full
  pl011_regs->ifls = IFSL_RX_1_2 | IFSL_TX_1_2;
  
  // Clear pending interrupts.
  pl011_regs->icr = INT_ALL;

  // Mask all interrupts.
  pl011_regs->imsc = INT_ALL;
  
  // Enable UART0 and enable tx and rx.
  pl011_regs->ctrl = CR_UARTEN | CR_TXW | CR_RXE;

  dmb();
}


/*
 *
 */
void hal_pl011_write_byte(char ch)
{
  while (pl011_regs->flags & FR_BUSY) {
  }
  mmio_write(&pl011_regs->data, ch);  
}


/*
 *
 */
bool hal_pl011_rx_ready(void)
{
  if (mmio_read(&pl011_regs->flags) & FR_RXFE) {
    return false;
  } else {
    return true;
  }
}


/*
 *
 */
bool hal_pl011_tx_ready(void)
{
  if (mmio_read(&pl011_regs->flags) & FR_TXFF) {
    return false;
  } else {
    return true;
  }
}



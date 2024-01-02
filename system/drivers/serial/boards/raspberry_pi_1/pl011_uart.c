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
#include <sys/syscalls.h>
#include <unistd.h>
#include <sys/fsreq.h>
#include <poll.h>
#include <sys/debug.h>
#include <sys/lists.h>
#include <task.h>
#include <sys/event.h>
#include "serial.h"
#include "globals.h"


/*
 *
 */
void interrupt_handler(int irq, struct InterruptAPI *api)
{    
#ifdef USE_INTERRUPTS
  dmb();
  api->MaskInterrupt(SERIAL_IRQ);
  api->EventNotifyFromISR(api, NOTE_INT | (NOTE_IRQMASK & irq));  
  dmb();
#endif
}


/*
 *
 */
void interrupt_clear(void)
{
#ifdef USE_INTERRUPTS
  dmb();
  uart->rsrecr = 0;
  uart->icr = 0xffffffff;
  dmb();
  unmaskinterrupt(SERIAL_IRQ);
#endif
}


/*
 *
 */
void uart_process_interrupts(void)
{
#ifdef USE_INTERRUPTS
  volatile uint32_t mis;
  
  dmb();    
  mis = uart->mis;      
  dmb();    

  if (mis & (INT_RXR | INT_RTR)) {
     taskwakeupall(&rx_rendez);
  }

  if (mis & INT_TXR) {
    taskwakeupall(&tx_rendez);
  }      

  interrupt_clear();
#else
  // Rely on a kevent timeout to call this function
  taskwakeupall(&rx_rendez);
  taskwakeupall(&tx_rendez);
#endif
}


/*
 *
 */
char uart_read_byte(void)
{
  char ch;
  
  ch = uart->data;
  return ch;
}


/*
 *
 */
void uart_write_byte(char ch)
{
  while (uart->flags & FR_BUSY) {
  }
  uart->data = ch;  
}


/*
 *
 */
bool uart_rx_ready(void)
{
  if (uart->flags & FR_RXFE) {
    return false;
  } else {
    return true;
  }
}


/*
 *
 */
bool uart_tx_ready(void)
{
  if (uart->flags & FR_TXFF) {
    return false;
  } else {
    return true;
  }
}



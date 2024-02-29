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

/*
 * Debugging functions and Debug() system call.
 */
  
#define KDEBUG

#include <kernel/board/arm.h>
#include <kernel/board/globals.h>
#include <kernel/board/gpio.h>
#include <kernel/board/aux_uart.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/filesystem.h>
#include <stdarg.h>

/*
 *
 */
void aux_uart_init(void)
{
  int baud = AUX_UART_BAUD;
 /* 
  hal_mmio_write(&aux_regs->enables, 1);
  hal_mmio_write(&aux_regs->mu_ier_reg, 0);
  hal_mmio_write(&aux_regs->mu_cntl_reg, 0);
  hal_mmio_write(&aux_regs->mu_lcr_reg, 3);
  hal_mmio_write(&aux_regs->mu_mcr_reg, 0);
  hal_mmio_write(&aux_regs->mu_ier_reg, 0);
  hal_mmio_write(&aux_regs->mu_iir_reg, 0xC6);
  hal_mmio_write(&aux_regs->mu_baud_reg, AUX_MU_BAUD(baud)); 

  // GPIO pins 14 and 15 must be configured as alternate function FN5 for Aux UART 
  configure_gpio(14, AUX_UART_GPIO_ALT_FN, PULL_NONE);
  configure_gpio(15, AUX_UART_GPIO_ALT_FN, PULL_NONE);

  hal_mmio_write(&aux_regs->mu_cntl_reg, 3);   //enable RX/TX
  */
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
//  while (!aux_uart_read_ready());
  return hal_mmio_read(&aux_regs->mu_io_reg);
}

void aux_uart_write_byte(char ch)
{
    while (!aux_uart_write_ready()); 
    hal_mmio_write(&aux_regs->mu_io_reg, ch);
}



/*
 * Copyright 2023  Marven Gilhespie
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



// BCM2835 Aux registers
static struct bcm2835_aux_registers *aux_regs;


/*
 *
 */
void hal_set_aux_base(void *base_addr)
{
  aux_regs = base_addr;
}


/*
 *
 */
void *hal_get_aux_base(void)
{
  return aux_regs;
}


/*
 *
 */
void hal_aux_uart_init(int baud)
{
  dmb();
  
  mmio_write(&aux->enables, 1);
  mmio_write(&aux->mu_ier_reg, 0);
  mmio_write(&aux->mu_cntl_reg, 0);
  mmio_write(&aux->mu_lcr_reg, 3);
  mmio_write(&aux->mu_mcr_reg, 0);
  mmio_write(&aux->mu_ier_reg, 0);
  mmio_write(&aux->mu_iir_reg, 0xC6);
  mmio_write(&aux->mu_baud_reg, AUX_MU_BAUD(baud)); 

  dmb();

  // select function 5 (AUX UART) and disable pull up/down for pins 14, 15
  configure_gpio(14, FN5, OFF);
  configure_gpio(15, FN5, OFF);

  mmio_write(&aux->mu_cntl_reg, 3);   //enable RX/TX
}


bool hal_aux_uart_read_ready(void)
{ 
  return (mmio_read(&aux->mu_lsr_reg) & 0x01) ? true : false;  

}


bool hal_aux_uart_write_ready(void)
{
  return (mmio_read(&aux->mu_lsr_reg) & 0x20) ? true : false;  
}


char hal_aux_uart_read_byte(void)
{
  return mmio_read(&aux->mu_io_reg);
}


void hal_aux_uart_write_byte(char ch)
{
    mmio_write(&aux->mu_io_reg, ch);
}



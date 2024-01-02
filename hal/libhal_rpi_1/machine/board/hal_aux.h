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

#ifndef MACHINE_BOARD_HAL_AUX_H
#define MACHINE_BOARD_HAL_AUX_H

#include <stdint.h>
#include <stdbool.h>


/*
 * FIXME: Define aux registers for bmc2835 of the Raspberry Pi 1
 */

#define AUX_UART_CLOCK 500000000
#define AUX_MU_BAUD(baud) ((AUX_UART_CLOCK/(baud*8))-1)


// FIXME: These are bcm2711 aux registers, UPDATE for bcm2835
struct bcm2835_aux_registers
{
  uint32_t aux_irq;
  uint32_t aux_enables;
  uint32_t resvd1[14];
  uint32_t aux_mu_io_reg;
  uint32_t aux_mu_ier_reg;
  uint32_t aux_mu_iir_reg;
  uint32_t aux_mu_lcr_reg;
  uint32_t aux_mu_mcr_reg;
  uint32_t aux_mu_lsr_reg;
  uint32_t aux_mu_cntl_reg;
  uint32_t aux_mu_baud_reg;
};


/*
 * Prototypes
 */
void hal_set_aux_base(void *base_addr);
void *hal_get_aux_base(void);
void hal_aux_uart_init(int baud);
bool hal_aux_uart_read_ready(void);
bool hal_aux_uart_write_ready(void);
char hal_aux_uart_read_byte(void);
void hal_aux_uart_write_byte(char ch);


#endif



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

#ifndef BOARDS_RASPBERRY_PI_4_AUX_UART_H
#define BOARDS_RASPBERRY_PI_4_AUX_UART_H

#include <stdint.h>
#include <stdbool.h>


// Constants and macros
//#define AUX_UART_IRQ            29
#define AUX_UART_GPIO_ALT_FN    FN5         // Use alternate function FN5 for GPIO 14 and 15
#define AUX_UART_CLOCK          500000000
#define AUX_MU_BAUD(baud)       ((AUX_UART_CLOCK/(baud*8))-1)


/* @brief   Aux mini-UART registers of the BCM2711
 */
struct bcm2711_aux_registers
{
  uint32_t irq;             // 0x00
  uint32_t enables;         // 0x04
  uint32_t resvd1[14];
  uint32_t mu_io_reg;       // 0x40
  uint32_t mu_ier_reg;      // 0x44
  uint32_t mu_iir_reg;      // 0x48
  uint32_t mu_lcr_reg;      // 0x4C
  uint32_t mu_mcr_reg;      // 0x50
  uint32_t mu_lsr_reg;      // 0x54
  uint32_t mu_msr_reg;      // 0x58
  uint32_t mu_scratch_reg;  // 0x5C
  uint32_t mu_cntl_reg;     // 0x60
  uint32_t mu_stat_reg;     // 0x64
  uint32_t mu_baud_reg;     // 0x68
  
  // SPI1 follows at 0x7E21 5080
} __attribute__((packed));

/* 
 * Register bit definitions
 */ 
enum {
  AUX_CNTL_RXEN	    = 0x01, /* Rx enable */
  AUX_CNTL_TXEN	    = 0x02, /* Tx enable */
  AUX_CNTL_AUTORTS  = 0x04, /* RTS set by Rx fill level */
  AUX_CNTL_AUTOCTS	= 0x08, /* CTS */
  AUX_CNTL_RTS4	    = 0x30, /* RTS set until 4 chars left */
  AUX_CNTL_RTS3	    = 0x00, /* RTS set until 3 chars left */
  AUX_CNTL_RTS2	    = 0x10, /* RTS set until 2 chars left */
  AUX_CNTL_RTS1	    = 0x20, /* RTS set until 1 char left */
  AUX_CNTL_RTSINV	  = 0x40, /* Invert RTS polarity */
  AUX_CNTL_CTSINV	  = 0x80  /* Invert CTS polarity */
};


// BCM2835 Aux registers
extern struct bcm2711_aux_registers *aux_regs;


/*
 * Prototypes
 */
void init_aux_uart(void);
bool aux_uart_read_ready(void);
bool aux_uart_write_ready(void);
char aux_uart_read_byte(void);
void aux_uart_write_byte(char ch);


#endif



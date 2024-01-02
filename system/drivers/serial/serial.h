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

#ifndef SERIAL_H
#define SERIAL_H

//#define NDEBUG

#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <sys/lists.h>
#include <sys/fsreq.h>
#include <sys/termios.h>
#include <sys/interrupts.h>
#include <sys/syscalls.h>
#include <sys/syslimits.h>
#include <task.h>


#define SERIAL_IRQ  57
#define INO_NR 0
#define PAGE_SIZE 4096
#define POLL_TIMEOUT 100000
#define NULL ((void *)0)

#if defined(BOARD_RASPBERRY_PI_1)

#define UART_CLK (3000000 * 16)

enum Base {
  GPIO_BASE_PA      = 0x00200000, // 0x??200000
  UART_BASE_PA      = 0x00201000, // 0x??201000
  IRQ_BASE          = 0x0000B000, // 0x??00B000
  TIMER_BASE        = 0x00003000, // 0x??003000
  BCM2835_PERI_BASE = 0x20000000,
};

struct bcm2835_uart_registers {
  uint32_t data;      // 0x00
  uint32_t rsrecr;    // 0x04
  uint32_t resvd1[4]; // 0x08  
  uint32_t flags;     // 0x18
  uint32_t resvd2;    // 0x1C
  uint32_t ilpr;      // 0x20
  uint32_t ibrd;      // 0x24
  uint32_t fbrd;      // 0x28
  uint32_t lcrh;      // 0x2C
  uint32_t ctrl;      // 0x30
  uint32_t ifls;      // 0x34
  uint32_t imsc;      // 0x38
  uint32_t ris;       // 0x3C
  uint32_t mis;       // 0x40
  uint32_t icr;       // 0x44
};

struct bcm2835_gpio_registers {
  uint32_t fsel[6];     // 0x00
  uint32_t resvd1;      // 0x18
  uint32_t set[2];      // 0x1C
  uint32_t resvd2;      // 0x24
  uint32_t clr[2];      // 0x28
  uint32_t resvd3[25];  // 0x30
  uint32_t pud;         // 0x94
  uint32_t pud_clk[2];  // 0x98
};

extern volatile struct bcm2835_uart_registers *uart;
extern volatile struct bcm2835_gpio_registers *gpio;

#elif defined(BOARD_RASPBERRY_PI_4)

#define UART_CLK (3000000 * 16)

enum Base {
  GPIO_BASE_PA      = 0x00200000, // 0x??200000
  UART_BASE_PA      = 0x00201000, // 0x??201000
//  IRQ_BASE          = 0x0000B000, // 0x??00B000
//  TIMER_BASE        = 0x00003000, // 0x??003000

  AUX_BASE_PA          = PERIPHERAL_BASE + 0x215000,
    AUX_ENABLES     = AUX_BASE + 4,
    AUX_MU_IO_REG   = AUX_BASE + 64,
    AUX_MU_IER_REG  = AUX_BASE + 68,
    AUX_MU_IIR_REG  = AUX_BASE + 72,
    AUX_MU_LCR_REG  = AUX_BASE + 76,
    AUX_MU_MCR_REG  = AUX_BASE + 80,
    AUX_MU_LSR_REG  = AUX_BASE + 84,
    AUX_MU_CNTL_REG = AUX_BASE + 96,
    AUX_MU_BAUD_REG = AUX_BASE + 104,
    AUX_UART_CLOCK  = 500000000,
    UART_MAX_QUEUE  = 16 * 1024

  BCM2711_PERI_BASE = 0xFE000000,
};


#define AUX_UART_CLOCK  = 500000000
#define UART_MAX_QUEUE  = 16 * 1024


struct bcm2711_aux_registers
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


struct bcm2711_uart_registers
{
  uint32_t data;      // 0x00
  uint32_t rsrecr;    // 0x04
  uint32_t resvd1[4]; // 0x08  
  uint32_t flags;     // 0x18
  uint32_t resvd2;    // 0x1C
  uint32_t ilpr;      // 0x20
  uint32_t ibrd;      // 0x24
  uint32_t fbrd;      // 0x28
  uint32_t lcrh;      // 0x2C
  uint32_t ctrl;      // 0x30
  uint32_t ifls;      // 0x34
  uint32_t imsc;      // 0x38
  uint32_t ris;       // 0x3C
  uint32_t mis;       // 0x40
  uint32_t icr;       // 0x44
};


struct bcm2711_gpio_registers
{
  uint32_t fsel[6];     // 0x00 
  uint32_t resvd1;      // 0x18
  uint32_t set[2];      // 0x1C
  uint32_t resvd2;      // 0x24
  uint32_t clr[2];      // 0x28
  uint32_t resvd3;      // 0x30
  uint32_t lev[2];      // 0x34
  uint32_t resvd4;      // 0x3c
  uint32_t eds[2];      // 0x40
  uint32_t resvd5;      // 0x48
  uint32_t ren[2];      // 0x4c
  uint32_t resvd6;      // 0x54
  uint32_t fen[2];      // 0x58
  uint32_t resvd7;      // 0x60
  uint32_t hen[2];      // 0x64
  uint32_t resvd8;      // 0x6c
  uint32_t len[2];      // 0x70
  uint32_t resvd9;      // 0x78
  uint32_t aren[2];     // 0x7c
  uint32_t resvd10;     // 0x84
  uint32_t afen[2];     // 0x88
  uint32_t resvd11[21];  // 0x90
  uint32_t pup_pdn_ctrl[4]; // 0xe4  
};


extern volatile struct bcm2711_aux_registers *aux;
extern volatile struct bcm2711_gpio_registers *gpio;

#else
#error "serial: unknown board"
#endif


enum FSel {
    INPUT, OUTPUT, FN5, FN4, FN0, FN1, FN2, FN3,
};
    
enum PullUpDown {
    OFF, UP, DOWN,
};


enum FlowControl {
  FLOW_CONTROL_NONE = 0,
  FLOW_CONTROL_SW   = 1,
  FLOW_CONTROL_HW   = 2,
};


enum {
    // Data register bits
    DR_OE = 1 << 11, // Overrun error
    DR_BE = 1 << 10, // Break error
    DR_PE = 1 <<  9, // Parity error
    DR_FE = 1 <<  8, // Framing error

    // Receive Status Register / Error Clear Register
    RSRECR_OE = 1 << 3, // Overrun error
    RSRECR_BE = 1 << 2, // Break error
    RSRECR_PE = 1 << 1, // Parity error
    RSRECR_FE = 1 << 0, // Framing error

    // Flag Register (depends on LCRH.FEN)
    FR_TXFE = 1 << 7, // Transmit FIFO empty
    FR_RXFF = 1 << 6, // Receive FIFO full
    FR_TXFF = 1 << 5, // Transmit FIFO full
    FR_RXFE = 1 << 4, // Receive FIFO empty
    FR_BUSY = 1 << 3, // BUSY transmitting data
    FR_CTS  = 1 << 0, // Clear To Send

    // Line Control Register
    LCRH_SPS   = 1 << 7, // sticky parity selected
    LCRH_WLEN  = 3 << 5, // word length (5, 6, 7 or 8 bit)
    LCRH_WLEN5 = 0 << 5, // word length 5 bit
    qLCRH_WLEN6 = 1 << 5, // word length 6 bit
    LCRH_WLEN7 = 2 << 5, // word length 7 bit
    LCRH_WLEN8 = 3 << 5, // word length 8 bit
    LCRH_FEN   = 1 << 4, // Enable FIFOs
    LCRH_STP2  = 1 << 3, // Two stop bits select
    LCRH_EPS   = 1 << 2, // Even Parity Select
    LCRH_PEN   = 1 << 1, // Parity enable
    LCRH_BRK   = 1 << 0, // send break

    // Control Register
    CR_CTSEN  = 1 << 15, // CTS hardware flow control
    CR_RTSEN  = 1 << 14, // RTS hardware flow control
    CR_RTS    = 1 << 11, // (not) Request to send
    CR_RXE    = 1 <<  9, // Receive enable
    CR_TXW    = 1 <<  8, // Transmit enable
    CR_LBE    = 1 <<  7, // Loopback enable
    CR_UARTEN = 1 <<  0, // UART enable

    // Interrupts (IMSC / RIS / MIS / ICR)
    INT_OER   = 1 << 10, // Overrun error interrupt
    INT_BER   = 1 <<  9, // Break error interrupt
    INT_PER   = 1 <<  8, // Parity error interrupt
    INT_FER   = 1 <<  7, // Framing error interrupt
    INT_RTR   = 1 <<  6, // Receive timeout interrupt
    INT_TXR   = 1 <<  5, // Transmit interrupt
    INT_RXR   = 1 <<  4, // Receive interrupt
    INT_DSRRM = 1 <<  3, // unsupported / write zero
    INT_DCDRM = 1 <<  2, // unsupported / write zero
    INT_CTSRM = 1 <<  1, // nUARTCTS modem interrupt
    INT_RIRM  = 1 <<  0, // unsupported / write zero
    INT_ALL = 0x7F2,

    IFSL_RXIFLSEL = 7 << 3,     // Receive interrupt FIFO level select
    IFSL_RX_1_8   = 0b000 << 3, // Receive FIFO 1/8 full
    IFSL_RX_1_4   = 0b001 << 3, // Receive FIFO 1/4 full
    IFSL_RX_1_2   = 0b010 << 3, // Receive FIFO 1/2 full
    IFSL_RX_3_4   = 0b011 << 3, // Receive FIFO 3/4 full
    IFSL_RX_7_8   = 0b100 << 3, // Receive FIFO 7/8 full
    IFSL_TXIFLSEL = 7 << 0,     // Transmit interrupt FIFO level select
    IFSL_TX_1_8   = 0b000 << 0, // Transmit FIFO 1/8 full
    IFSL_TX_1_4   = 0b001 << 0, // Transmit FIFO 1/4 full
    IFSL_TX_1_2   = 0b010 << 0, // Transmit FIFO 1/2 full
    IFSL_TX_3_4   = 0b011 << 0, // Transmit FIFO 3/4 full
    IFSL_TX_7_8   = 0b100 << 0, // Transmit FIFO 7/8 full
};


/*
 * Configuration settings
 */
struct Config
{
  char pathname[PATH_MAX + 1];
  uid_t uid;
  gid_t gid;
  mode_t mode;

  int baud;
  int flow_control;
  int stop_bits;
  bool parity;
};


// prototypes
void cmd_isatty(msgid_t msgid, struct fsreq *req);
void cmd_read(msgid_t msgid, struct fsreq *req);
void cmd_write(msgid_t msgid, struct fsreq *req);
void cmd_tcsetattr(msgid_t msgid, struct fsreq *req);
void cmd_tcgetattr(msgid_t msgid, struct fsreq *req);

void init(int argc, char *argv[]);
int process_args(int argc, char *argv[]);
int mount_device(void);

void configure_gpio(uint32_t pin, enum FSel fn, enum PullUpDown action);
void set_gpio(uint32_t pin, bool state);
int configure_uart(void);

void io_delay(uint32_t cycles);

void reader_task(void *arg);
void writer_task(void *arg);
void uart_tx_task(void *arg);
void uart_rx_task(void *arg);

void line_discipline(uint8_t ch);
int get_line_length(void);
void echo(uint8_t ch);

void interrupt_handler(int irq, struct InterruptAPI *api);
void interrupt_clear(void);

void Delay(uint32_t cycles);

void isb(void);
void dsb(void);
void dmb(void);

void input_processing(uint8_t ch);



#endif


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

#define NDEBUG

#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <sys/lists.h>
#include <sys/fsreq.h>
#include <sys/termios.h>
#include <sys/interrupts.h>
#include <sys/syslimits.h>
#include <task.h>


#define PAGE_SIZE 4096

#define POLL_TIMEOUT 100000

#define ALIGN_UP(val, alignment)                                               \
  ((((val) + (alignment)-1) / (alignment)) * (alignment))

#define ALIGN_DOWN(val, alignment) ((val) - ((val) % (alignment)))

#define UART_CLK (3000000 * 16)
#define NULL ((void *)0)

#define SERIAL_IRQ  57

#define INO_NR 0


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


enum Base {
    NONE       = 0,
    GPIO_BASE_PA  = 0x200000, // 0x??200000
    UART_BASE_PA = 0x201000, // 0x??201000
    IRQ_BASE   = 0x00B000, // 0x??00B000
    TIMER_BASE = 0x003000, // 0x??003000
    BCM2708_PERI_BASE = 0x20000000,
};



struct bcm2835_uart_registers {
  uint32_t data;      // 0x00 - 0x04
  uint32_t rsrecr;    // 0x04 - 0x08
  uint32_t resvd1[4]; // 0x08 - 0x18  
  uint32_t flags;     // 0x18 - 0x1C
  uint32_t resvd2;    // 0x1C - 0x20
  uint32_t ilpr;      // 0x20 - 0x24
  uint32_t ibrd;      // 0x24 - 0x28
  uint32_t fbrd;      // 0x28 - 0x2C
  uint32_t lcrh;      // 0x2C - 0x30
  uint32_t ctrl;      // 0x30 - 0x34
  uint32_t ifls;      // 0x34 - 0x38
  uint32_t imsc;      // 0x38 - 0x3C
  uint32_t ris;       // 0x3C - 0x40
  uint32_t mis;       // 0x40 - 0x44
  uint32_t icr;       // 0x44 - 0x48
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



struct bcm2835_gpio_registers {
  uint32_t fsel[6];     // 0x00 - 0x14
  uint32_t resvd1;      // 0x18 - 0x1C
  uint32_t set[2];      // 0x1C - 0x024
  uint32_t resvd2;      // 0x24 - 0x28
  uint32_t clr[2];      // 0x28 - 0x30
  uint32_t resvd3[25];  // 0x30 - 0x94
  uint32_t pud;         // 0x94 - 0x98
  uint32_t pud_clk[2];  // 0x98 - 0x10
};





// types

struct Config {
  char pathname[PATH_MAX + 1];
  uid_t uid;
  gid_t gid;
  mode_t mode;

  int baud;
  int flow_control;
  int stop_bits;
  bool parity;
};



// globals

extern int fd;
extern int interrupt_fd;

extern int sid;

extern int tx_head;
extern int tx_sz;
extern int tx_free_head;
extern int tx_free_sz;
extern uint8_t tx_buf[4096];

extern int rx_head;
extern int rx_sz;
extern int rx_free_head;
extern int rx_free_sz;
extern uint8_t rx_buf[4096];

extern int line_cnt;
extern int line_end;


extern bool write_pending;
extern bool read_pending;
extern int read_msgid;
extern int write_msgid;

extern Rendez tx_rendez;
extern Rendez rx_rendez;

extern Rendez tx_free_rendez;
extern Rendez rx_data_rendez;

extern Rendez write_cmd_rendez;
extern Rendez read_cmd_rendez;

extern struct fsreq read_fsreq;
extern struct fsreq write_fsreq;



extern struct termios termios;
extern volatile struct bcm2835_uart_registers *uart;
extern volatile struct bcm2835_gpio_registers *gpio;

extern struct Config config;






// prototypes
void cmd_isatty(int sid, struct fsreq *req);
void cmd_read(int sid, struct fsreq *req);
void cmd_write(int sid, struct fsreq *req);
void cmd_tcsetattr(int sid, struct fsreq *req);
void cmd_tcgetattr(int sid, struct fsreq *req);

void init (int argc, char *argv[]);
int process_args(int argc, char *argv[]);
int mount_device(void);

void configure_gpio(uint32_t pin, enum FSel fn, enum PullUpDown action);
void set_gpio(uint32_t pin, bool state);
void configure_uart(void);

void io_delay(uint32_t cycles);

void reader_task (void *arg);
void writer_task (void *arg);


void uart_tx_task(void *arg);
void uart_rx_task(void *arg);

void line_discipline(uint8_t ch);

void echo(uint8_t ch);

void interrupt_handler(int irq, struct InterruptAPI *api);
void interrupt_clear(void);

void Delay(uint32_t cycles);

void isb(void);
void dsb(void);

void dmb(void);

void input_processing(uint8_t ch);



#endif


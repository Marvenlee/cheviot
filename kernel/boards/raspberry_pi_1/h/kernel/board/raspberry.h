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

#ifndef KERNEL_ARM_RASPBERRY_H
#define KERNEL_ARM_RASPBERRY_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

/*
 * Physical addresses of chipset peripherals
 */

#define BCM2708_PERI_BASE 0x20000000

#define IC0_BASE (BCM2708_PERI_BASE + 0x2000)

#define ST_BASE (BCM2708_PERI_BASE + 0x3000) /* System Timer */

#define GPIO_BASE (BCM2708_PERI_BASE + 0x200000) /* GPIO */

#define UART_BASE (BCM2708_PERI_BASE + 0x201000) /* GPIO */

#define ARM_BASE (BCM2708_PERI_BASE + 0xB000) /* BCM2708 ARM control block */

#define ARMCTRL_BASE (ARM_BASE + 0x000)
#define ARMCTRL_IC_BASE (ARM_BASE + 0x200) /* ARM interrupt controller */

/*
 * Interrupt assignments
 */

#define ARM_IRQ1_BASE 0
#define INTERRUPT_TIMER0 (ARM_IRQ1_BASE + 0)
#define INTERRUPT_TIMER1 (ARM_IRQ1_BASE + 1)
#define INTERRUPT_TIMER2 (ARM_IRQ1_BASE + 2)
#define INTERRUPT_TIMER3 (ARM_IRQ1_BASE + 3)

#define NIRQ (32 + 32 + 20)

/*
 * GPIO Registers
 */

/*
struct bcm2835_gpio_registers {
  uint32_t gpfsel0;
  uint32_t gpfsel1;
  uint32_t gpfsel2;
  uint32_t gpfsel3;
  uint32_t gpfsel4;
  uint32_t gpfsel5;

  uint32_t resvd1;

  uint32_t gpset0;
  uint32_t gpset1;

  uint32_t resvd2;

  uint32_t gpclr0;
  uint32_t gpclr1;

  uint32_t gplev0;
  uint32_t gplev1;
};
*/

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



#define UART_CLK (3000000 * 16)


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
 * System Timer
 */

struct bcm2835_system_timer_registers {
  uint32_t cs;
  uint32_t clo;
  uint32_t chi;
  uint32_t c0;
  uint32_t c1;
  uint32_t c2;
  uint32_t c3;
};

#define ST_CS_M3 (0x08)
#define ST_CS_M2 (0x04)
#define ST_CS_M1 (0x02)
#define ST_CS_M0 (0x01)

/*
 * Interupt controller
 */

struct bcm2835_interrupt_registers {
  uint32_t irq_basic_pending;
  uint32_t irq_pending_1;
  uint32_t irq_pending_2;
  uint32_t fiq_control;
  uint32_t enable_irqs_1;
  uint32_t enable_irqs_2;
  uint32_t enable_basic_irqs;
  uint32_t disable_irqs_1;
  uint32_t disable_irqs_2;
  uint32_t disable_basic_irqs;
};

/*
 * Prototypes
 */

void MailBoxWrite(uint32_t v, uint32_t id);
uint32_t MailBoxRead(uint32_t id);
uint32_t MailBoxStatus();

void GetArmMemory(void);
void GetVideocoreMemory(void);
uint16_t CalcColor(uint8_t r, uint8_t g, uint8_t b);
void PutPixel(int x, int y, uint16_t color);
uint32_t *FindTag(uint32_t tag, uint32_t *msg);
void BlankScreen(void);
void GetScreenDimensions(void);
void SetScreenMode(void);

void LedOn(void);
void LedOff(void);
void BlinkLEDs(int cnt);

void EnableIRQ(int irq);
void DisableIRQ(int irq);

#endif

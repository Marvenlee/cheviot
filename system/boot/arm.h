#ifndef ARM_H
#define ARM_H

#include "types.h"
#include <stdint.h>

/*
 * VM types
 */

typedef int spinlock_t;
typedef uint32 vm_addr;
typedef uint32 vm_offset;
typedef uint32 vm_size;
typedef uint32 pte_t;
typedef uint32 pde_t;

/*
 * iflag_t used to store interrupt enabled/disabled flag state
 */

typedef unsigned long iflag_t;

// Constants
#define ROOT_PAGETABLES_CNT 1
#define ROOT_PAGETABLES_PDE_BASE 0

#define IO_PAGETABLES_CNT 16
#define IO_PAGETABLES_PDE_BASE 2560

#define KERNEL_PAGETABLES_CNT 512
#define KERNEL_PAGETABLES_PDE_BASE 2048

#define ROOT_CEILING_ADDR 0x00010000
#define KERNEL_BASE_VA 0x80000000
#define IOMAP_BASE_VA 0xA0000000

// Defines

#define PAGE_SIZE 4096
#define VPAGETABLE_SZ 4096
#define VPTE_TABLE_OFFS 1024
#define PAGEDIR_SZ 16384

#define N_PAGEDIR_PDE 4096
#define N_PAGETABLE_PTE 256

#define VPTE_TABLE_OFFS 1024

/*
 * L1 - Page Directory Entries
 */

#define L1_ADDR_BITS 0xfff00000 /* L1 PTE address bits */
#define L1_IDX_SHIFT 20
#define L1_TABLE_SIZE 0x4000 /* 16K */

#define L1_TYPE_INV 0x00  /* Invalid   */
#define L1_TYPE_C 0x01    /* Coarse    */
#define L1_TYPE_S 0x02    /* Section   */
#define L1_TYPE_F 0x03    /* Fine      */
#define L1_TYPE_MASK 0x03 /* Type Mask */

#define L1_S_B 0x00000004      /* Bufferable Section */
#define L1_S_C 0x00000008      /* Cacheable Section  */
#define L1_S_AP(x) ((x) << 10) /* Access Permissions */

#define L1_S_ADDR_MASK 0xfff00000 /* Address of Section  */
#define L1_C_ADDR_MASK 0xfffffc00 /* Address of L2 Table */

/*
 * L2 - Page Table Entries
 */

#define L2_ADDR_MASK 0xfffff000 /* L2 PTE Address mask of page */
#define L2_ADDR_BITS 0x000ff000 /* L2 PTE address bits */
#define L2_IDX_SHIFT 12
#define L2_TABLE_SIZE 0x0400 /* 1K */

#define L2_TYPE_INV 0x00 /* PTE Invalid     */
#define L2_NX 0x01       /* No Execute bit */
#define L2_TYPE_S 0x02   /* PTE ARMv6 4k Small Page  */

#define L2_B 0x00000004 /* Bufferable Page */
#define L2_C 0x00000008 /* Cacheable Page  */

#define L2_AP(x) ((x) << 4)  // 2 bit access permissions
#define L2_TEX(x) ((x) << 6) // 3 bit memory-access ordering
#define L2_APX (1 << 9)      // Access permissions (see table in arm manual)
#define L2_S (1 << 10)  // shared by other processors (used for page tables?)
#define L2_NG (1 << 11) // Non-Global (when set uses ASID)

//
#define C1_XP (1 << 23) // 0 subpage enabled (arm v5 mode)

/*
 * Access Permissions
 */

#define AP_W 0x01 /* Writable */
#define AP_U 0x02 /* User     */

#define AP_KR 0x00     /* kernel read */
#define AP_KRW 0x01    /* kernel read/write */
#define AP_KRWUR 0x02  /* kernel read/write usr read */
#define AP_KRWURW 0x03 /* kernel read/write usr read/write */

#define BCM2708_PERI_BASE 0x20000000
#define IC0_BASE (BCM2708_PERI_BASE + 0x2000)
#define ST_BASE (BCM2708_PERI_BASE + 0x3000)     /* System Timer */
#define GPIO_BASE (BCM2708_PERI_BASE + 0x200000) /* GPIO */
#define ARM_BASE (BCM2708_PERI_BASE + 0xB000)    /* BCM2708 ARM control block */
#define ARMCTRL_BASE (ARM_BASE + 0x000)
#define ARMCTRL_IC_BASE (ARM_BASE + 0x200) /* ARM interrupt controller */


#define BCM2708_PERI_BASE 0x20000000

#define IC0_BASE (BCM2708_PERI_BASE + 0x2000)

#define ST_BASE (BCM2708_PERI_BASE + 0x3000) /* System Timer */

#define GPIO_BASE (BCM2708_PERI_BASE + 0x200000) /* GPIO */

#define UART_BASE (BCM2708_PERI_BASE + 0x201000) /* GPIO */

#define ARM_BASE (BCM2708_PERI_BASE + 0xB000) /* BCM2708 ARM control block */

#define ARMCTRL_BASE (ARM_BASE + 0x000)
#define ARMCTRL_IC_BASE (ARM_BASE + 0x200) /* ARM interrupt controller */


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
 * Interupt controller
 */
struct bcm2835_interrupt_registers {
  uint32 irq_basic_pending;
  uint32 irq_pending_1;
  uint32 irq_pending_2;
  uint32 fiq_control;
  uint32 enable_irqs_1;
  uint32 enable_irqs_2;
  uint32 enable_basic_irqs;
  uint32 disable_irqs_1;
  uint32 disable_irqs_2;
  uint32 disable_basic_irqs;
};

/*
 * Prototypes
 */

void MailBoxWrite(uint32 v, uint32 id);
uint32 MailBoxRead(uint32 id);
uint32 MailBoxStatus();
__attribute__((naked)) void MemoryBarrier();
__attribute__((naked)) void DataCacheFlush();
__attribute__((naked)) void SynchronisationBarrier();
__attribute__((naked)) void DataSynchronisationBarrier();

void GetArmMemory(void);
void GetVideocoreMemory(void);
uint16 CalcColor(uint8 r, uint8 g, uint8 b);
void PutPixel(int x, int y, uint16 color);
uint32 *FindTag(uint32 tag, uint32 *msg);
void BlankScreen(void);
void GetScreenDimensions(void);
void SetScreenMode(void);

void LedOn(void);
void LedOff(void);
void BlinkLEDs(int cnt);

uint32 GetCPSR(void);
uint32 GetSPSR(void);

#endif

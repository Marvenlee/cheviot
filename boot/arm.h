#ifndef ARM_H
#define ARM_H


#include "types.h"


/*
 * VM types
 */


typedef int 		spinlock_t;
typedef uint32		vm_addr;
typedef uint32		vm_offset;
typedef uint32		vm_size;
typedef uint32		pte_t;
typedef uint32      pde_t;

/*
 * iflag_t used to store interrupt enabled/disabled flag state
 */
 
typedef unsigned long iflag_t;

// Defines

#define PAGE_SIZE           4096
#define VPAGETABLE_SZ       4096
#define VPTE_TABLE_OFFS     1024
#define PAGEDIR_SZ          16384

#define N_PAGEDIR_PDE       4096     
#define N_PAGETABLE_PTE     256     

#define VPTE_TABLE_OFFS     1024


// Addresses

#define KERNEL_BASE_VA      0x80000000
#define IOMAP_BASE_VA       0xA0000000



/*
 * L1 - Page Directory Entries
 */

#define L1_ADDR_BITS    0xfff00000  /* L1 PTE address bits */
#define L1_IDX_SHIFT    20
#define L1_TABLE_SIZE   0x4000      /* 16K */

#define L1_TYPE_INV     0x00        /* Invalid   */
#define L1_TYPE_C       0x01        /* Coarse    */
#define L1_TYPE_S       0x02        /* Section   */
#define L1_TYPE_F       0x03        /* Fine      */
#define L1_TYPE_MASK    0x03        /* Type Mask */

#define L1_S_B          0x00000004  /* Bufferable Section */
#define L1_S_C          0x00000008  /* Cacheable Section  */
#define L1_S_AP(x)      ((x) << 10) /* Access Permissions */

#define L1_S_ADDR_MASK  0xfff00000  /* Address of Section  */
#define L1_C_ADDR_MASK  0xfffffc00  /* Address of L2 Table */

/*
 * L2 - Page Table Entries
 */

#define L2_ADDR_BITS    0x000ff000  /* L2 PTE address bits */
#define L2_IDX_SHIFT    12
#define L2_TABLE_SIZE   0x0400      /* 1K */

#define L2_TYPE_INV     0x00        /* PTE Invalid     */
#define L2_TYPE_L       0x01        /* PTE Large page  */
#define L2_TYPE_S       0x02        /* PTE Small Page  */
#define L2_TYPE_MASK    0x03

#define L2_B        0x00000004      /* Bufferable Page */
#define L2_C        0x00000008      /* Cacheable Page  */
#define L2_AP(x)    (((x) << 4) | ((x) << 6) | ((x) << 8) | ((x) << 10))

// 
#define C1_XP           (1<<23)       // 0 subpage enabled (arm v5 mode)

/*
 * Access Permissions
 */

#define AP_W            0x01        /* Writable */
#define AP_U            0x02        /* User     */

#define AP_KR           0x00        /* kernel read */
#define AP_KRW          0x01        /* kernel read/write */
#define AP_KRWUR        0x02        /* kernel read/write usr read */
#define AP_KRWURW       0x03        /* kernel read/write usr read/write */

#define BCM2708_PERI_BASE        0x20000000
#define IC0_BASE                 (BCM2708_PERI_BASE + 0x2000)
#define ST_BASE                  (BCM2708_PERI_BASE + 0x3000)   /* System Timer */
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO */
#define ARM_BASE                 (BCM2708_PERI_BASE + 0xB000)   /* BCM2708 ARM control block */
#define ARMCTRL_BASE             (ARM_BASE + 0x000)
#define ARMCTRL_IC_BASE          (ARM_BASE + 0x200)             /* ARM interrupt controller */


/*
 * GPIO
 */
struct bcm2835_gpio_registers
{
    uint32 gpfsel0;
    uint32 gpfsel1;
    uint32 gpfsel2;
    uint32 gpfsel3;
    uint32 gpfsel4;
    uint32 gpfsel5;
    uint32 resvd1;
    uint32 gpset0;
    uint32 gpset1;
    uint32 resvd2;
    uint32 gpclr0;
    uint32 gpclr1;
    uint32 gplev0;
    uint32 gplev1;
};


/*
 * System Timer
 */
struct bcm2835_system_timer_registers
{
    uint32 cs;
    uint32 clo;
    uint32 chi;
    uint32 c0;
    uint32 c1;
    uint32 c2;
    uint32 c3;
};

/*
 * Interupt controller
 */
struct bcm2835_interrupt_registers
{
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

void MailBoxWrite (uint32 v, uint32 id);
uint32 MailBoxRead(uint32 id);
uint32 MailBoxStatus();
__attribute__((naked)) void MemoryBarrier();
__attribute__((naked)) void DataCacheFlush();
__attribute__((naked)) void SynchronisationBarrier();
__attribute__((naked)) void DataSynchronisationBarrier();

void GetArmMemory (void);
void GetVideocoreMemory (void);
uint16 CalcColor (uint8 r, uint8 g, uint8 b);
void PutPixel (int x, int y, uint16 color);
uint32 *FindTag (uint32 tag, uint32 *msg);
void BlankScreen(void);
void GetScreenDimensions(void);
void SetScreenMode(void);

void LedOn (void);
void LedOff (void);
void BlinkLEDs (int cnt);







#endif

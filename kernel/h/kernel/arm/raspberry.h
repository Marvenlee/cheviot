#ifndef KERNEL_ARM_RASPBERRY_H
#define KERNEL_ARM_RASPBERRY_H







/*
 * Physical addresses of chipset peripherals
 */
 
#define BCM2708_PERI_BASE        0x20000000

#define IC0_BASE                 (BCM2708_PERI_BASE + 0x2000)

#define ST_BASE                  (BCM2708_PERI_BASE + 0x3000)   /* System Timer */

#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO */

#define ARM_BASE                 (BCM2708_PERI_BASE + 0xB000)    /* BCM2708 ARM control block */

#define ARMCTRL_BASE             (ARM_BASE + 0x000)
#define ARMCTRL_IC_BASE          (ARM_BASE + 0x200)           /* ARM interrupt controller */



/*
 * Interrupt assignments
 */

#define ARM_IRQ1_BASE                  0
#define INTERRUPT_TIMER0               (ARM_IRQ1_BASE + 0)
#define INTERRUPT_TIMER1               (ARM_IRQ1_BASE + 1)
#define INTERRUPT_TIMER2               (ARM_IRQ1_BASE + 2)
#define INTERRUPT_TIMER3               (ARM_IRQ1_BASE + 3)

#define NIRQ                      (32 + 32 + 20)



/*
 * GPIO Registers
 */

struct bcm2835_gpio_registers
{
    uint32 gpfsel0;
    uint32 gpfsel1;
    uint32 pad1[5];
    uint32 gpset0;
    uint32 gpclr0;
    uint32 pad2;
    uint32 gpset1;
    uint32 gpclr1;
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

#define ST_CS_M3            (0x08)
#define ST_CS_M2            (0x04)
#define ST_CS_M1            (0x02)
#define ST_CS_M0            (0x01)





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


void EnableIRQ (int irq);
void DisableIRQ (int irq);




#endif


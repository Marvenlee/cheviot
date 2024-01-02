#include "dbg.h"
#include "globals.h"
#include "string.h"
#include "types.h"
#include <stdarg.h>
#include <sys/syscalls.h>
#include "arm.h"



volatile struct bcm2835_gpio_registers *gpio_regs;
volatile struct bcm2835_uart_registers *uart_regs;





/*
 * This debugger is part of the init process
 *
 * DO NOT Use these functions once paging is enabled or from the root task.
 */

static void KPrintString(char *s);
void io_delay(uint32_t cycles);
void configure_gpio(uint32_t pin, enum FSel fn, enum PullUpDown action);
void set_gpio(uint32_t pin, bool state);
void configure_uart(void);

#define KLOG_WIDTH 80

char klog_entry[KLOG_WIDTH];

/*!
    Initialise debug log buffer and clear screen
 */
void dbg_init(void) {
  BlinkLEDs(10);
  configure_uart();
}

void io_delay(uint32_t cycles) 
{
    while(cycles-- > 0) isb();
}

void configure_gpio(uint32_t pin, enum FSel fn, enum PullUpDown action)
{    
    dmb();
    
    
    
    
    // set pull up down
    // ----------------
    
    // set action & delay for 150 cycles.
    volatile uint32_t *pud = &gpio_regs->pud;
    *pud = action;
    io_delay(1000);

    // trigger action & delay for 150 cycles.
    volatile uint32_t *clock = &gpio_regs->pud_clk[pin / 32];
    *clock = (1 << (pin % 32));
    io_delay(1000);
    
    // clear action
    *pud = OFF;
	
    // remove clock
    *clock = 0;

    // set function
    // ------------
    volatile uint32_t *fsel = &gpio_regs->fsel[pin / 10];
    uint32_t shift = (pin % 10) * 3;
    uint32_t mask = ~(7U << shift);
    *fsel = (*fsel & mask) | (fn << shift);
    
    dmb();
}

void set_gpio(uint32_t pin, bool state)
{
    // set or clear output of pin
    
    if (state) {
      gpio_regs->set[pin / 32] = 1U << (pin % 32);
    } else {
      gpio_regs->clr[pin / 32] = 1U << (pin % 32);
    }
}

void configure_uart(void)
{
    dmb();


    gpio_regs = (struct bcm2835_gpio_registers *) GPIO_BASE;
    uart_regs = (struct bcm2835_uart_registers *) UART_BASE;
    
    // wait for end of transmission
    while(uart_regs->flags & FR_BUSY) { }

    // Disable UART0
    uart_regs->ctrl = 0;

    // flush transmit FIFO
    uart_regs->lcrh &= ~LCRH_FEN;	
	
    // select function 0 and disable pull up/down for pins 14, 15
    configure_gpio(14, FN0, OFF);
    configure_gpio(15, FN0, OFF);

    dmb();
    
    // Set integer & fractional part of baud rate.
    // Divider = UART_CLOCK/(16 * Baud)
    // Fraction part register = (Fractional part * 64) + 0.5
    // UART_CLOCK = 3000000; Baud = 115200.

    // Divider = 3000000/(16 * 115200) = 1.627 = ~1.
    // Fractional part register = (.627 * 64) + 0.5 = 40.6 = ~40.
  
    int baud = 115200;

    int divider = (UART_CLK)/(16 * baud);
    int temp = (((UART_CLK % (16 * baud)) * 8) / baud);
    int fraction = (temp >> 1) + (temp & 1);
    
    uart_regs->ibrd = divider;
    uart_regs->fbrd = fraction;

    // Enable FIFO & 8 bit data transmission (1 stop bit, no parity)
    uart_regs->lcrh = LCRH_FEN | LCRH_WLEN8;
    uart_regs->lcrh = LCRH_WLEN8;

    // set FIFO interrupts to fire at half full
    uart_regs->ifls = IFSL_RX_1_2 | IFSL_TX_1_2;
    
    // Clear pending interrupts.
    uart_regs->icr = INT_ALL;

    // Mask all interrupts.
    uart_regs->imsc = INT_ALL;
    
    // Enable UART0, receive & transfer part of UART.
//    uart_regs->ctrl = CR_UARTEN | CR_RXE;

    uart_regs->ctrl = CR_UARTEN | CR_TXW | CR_RXE;

    dmb();
}

/*
    Print panic message and halt boot process
*/
void KPanic(char *string) {
  if (string != NULL) {
    KLOG("%s", string);
  }

  while (1)
    ;
}

void KLog(const char *format, ...) {
  va_list ap;

  va_start(ap, format);

  Vsnprintf(&klog_entry[0], KLOG_WIDTH, format, ap);
  KPrintString(&klog_entry[0]);

  va_end(ap);
}

static void KPrintString(char *s) {  
  while (*s != '\0') {
    while (uart_regs->flags & FR_BUSY);        
    uart_regs->data = *s++;
  }
  
  while (uart_regs->flags & FR_BUSY);        
  uart_regs->data = '\n';
}

/*!
    Blink LEDs continuously,
*/
void BlinkError(void) {
  int c;

  while (1) {
    LedOn();

    for (c = 0; c < 1600000; c++)
      ;

    LedOff();

    for (c = 0; c < 800000; c++)
      ;
  }
}

/*!
*/
uint32 *FindTag(uint32 tag, uint32 *msg) {
  uint32 skip_size;

  msg += 2;

  while (*msg != 0 && *msg != tag) {
    skip_size = (*(msg + 1) + 12) / 4;
    msg += skip_size;
  }

  if (*msg == 0)
    return NULL;

  return msg;
}

/*!
*/
void BlankScreen(void) {
  uint32 result;

  mailbuffer[0] = 7 * 4;
  mailbuffer[1] = 0;
  mailbuffer[2] = 0x00040002;
  mailbuffer[3] = 4;
  mailbuffer[4] = 0;
  mailbuffer[5] = 0;
  mailbuffer[6] = 0;

  do {
    MailBoxWrite((uint32)mailbuffer, 8);
    result = MailBoxRead(8);
  } while (result == 0);
}

/*!
*/
void GetScreenDimensions(void) {
  uint32 result;

  screen_width = 640;
  screen_height = 480;

  mailbuffer[0] = 8 * 4;
  mailbuffer[1] = 0;
  mailbuffer[2] = 0x00040003;
  mailbuffer[3] = 8;
  mailbuffer[4] = 8;
  mailbuffer[5] = 0;
  mailbuffer[6] = 0;
  mailbuffer[7] = 0;

  do {
    MailBoxWrite((uint32)mailbuffer, 8);
    result = MailBoxRead(8);
  } while (result == 0);

  screen_width = mailbuffer[5];
  screen_height = mailbuffer[6];
}

/*!
*/
void SetScreenMode(void) {
  uint32 result;
  uint32 *tva;

  mailbuffer[0] = 22 * 4;
  mailbuffer[1] = 0;
  mailbuffer[2] = 0x00048003; // Physical size
  mailbuffer[3] = 8;
  mailbuffer[4] = 8;
  mailbuffer[5] = screen_width;
  mailbuffer[6] = screen_height;
  mailbuffer[7] = 0x00048004; // Virtual size
  mailbuffer[8] = 8;
  mailbuffer[9] = 8;
  mailbuffer[10] = screen_width;
  mailbuffer[11] = screen_height;
  mailbuffer[12] = 0x00048005; // Depth
  mailbuffer[13] = 4;
  mailbuffer[14] = 4;
  mailbuffer[15] = 16;         // 16 bpp
  mailbuffer[16] = 0x00040001; // Allocate buffer
  mailbuffer[17] = 8;
  mailbuffer[18] = 8;
  mailbuffer[19] = 16; // alignment
  mailbuffer[20] = 0;
  mailbuffer[21] = 0;

  do {
    MailBoxWrite((uint32)mailbuffer, 8);
    result = MailBoxRead(8);
  } while (result == 0);

  if (mailbuffer[1] != 0x80000000)
    BlinkError();

  tva = FindTag(0x00040001, mailbuffer);

  if (tva == NULL)
    BlinkError();

  screen_buf = (void *)*(tva + 3);

  mailbuffer[0] = 8 * 4;
  mailbuffer[1] = 0;
  mailbuffer[2] = 0x00040008; // Get Pitch
  mailbuffer[3] = 4;
  mailbuffer[4] = 0;
  mailbuffer[5] = 0;
  mailbuffer[6] = 0; // End Tag
  mailbuffer[7] = 0;

  do {
    MailBoxWrite((uint32)mailbuffer, 8);
    result = MailBoxRead(8);
  } while (result == 0);

  tva = FindTag(0x00040008, mailbuffer);

  if (tva == NULL)
    BlinkError();

  screen_pitch = *(tva + 3);
}

/*!
*/
uint16 CalcColor(uint8 r, uint8 g, uint8 b) {
  uint16 c;

  b >>= 5;
  g >>= 5;
  r >>= 5;

  c = (r << 11) | (g << 5) | (b);
  return c;
}

/*!
*/
void PutPixel(int x, int y, uint16 color) {
  *(uint16 *)((vm_addr)screen_buf + y * (screen_pitch) + x * 2) = color;
}

/*!
*/
void MailBoxWrite(uint32 v, uint32 id) {
  uint32 s;

  MemoryBarrier();
  DataCacheFlush();

  while (1) {
    s = MailBoxStatus();

    if ((s & 0x80000000) == 0)
      break;
  }

  MemoryBarrier();
  *((uint32 *)(0x2000b8a0)) = v | id;
  MemoryBarrier();
}

/*!
*/
uint32 MailBoxRead(uint32 id) {
  uint32 s;
  volatile uint32 v;

  MemoryBarrier();
  DataCacheFlush();

  while (1) {
    while (1) {
      s = MailBoxStatus();
      if ((s & 0x40000000) == 0)
        break;
    }

    MemoryBarrier();

    v = *((uint32 *)(0x2000b880));

    if ((v & 0x0000000f) == id)
      break;
  }
  return v & 0xfffffff0;
}

/*!
*/
uint32 MailBoxStatus() {
  volatile uint32 v;
  MemoryBarrier();
  DataCacheFlush();
  v = *((uint32 *)(0x2000b898));
  MemoryBarrier();
  return v;
}

/*!
*/
__attribute__((naked)) void MemoryBarrier() {
  __asm("mov r0, #0");
  __asm("mcr p15, #0, r0, c7, c10, #5");
  __asm("mov pc, lr");
}

__attribute__((naked)) void DataCacheFlush() {
  __asm("mov r0, #0");
  __asm("mcr p15, #0, r0, c7, c14, #0");
  __asm("mov pc, lr");
}

__attribute__((naked)) void SynchronisationBarrier() {
  __asm("mov r0, #0");
  __asm("mcr p15, #0, r0, c7, c10, #4");
  __asm("mov pc, lr");
}

__attribute__((naked)) void DataSynchronisationBarrier() {
  __asm("stmfd sp!, {r0-r8,r12,lr}");
  __asm("mcr p15, #0, ip, c7, c5, #0");
  __asm("mcr p15, #0, ip, c7, c5, #6");
  __asm("mcr p15, #0, ip, c7, c10, #4");
  __asm("mcr p15, #0, ip, c7, c10, #4");
  __asm("ldmfd sp!, {r0-r8,r12,pc}");
}

/*!
*/
void LedOn(void) {
  gpio_regs->fsel[1] = (1 << 18);
  gpio_regs->clr[0] = (1 << 16);
}

/*!
*/
void LedOff(void) {
  gpio_regs->fsel[1] = (1 << 18);
  gpio_regs->set[0] = (1 << 16);
}

/*!
*/

static volatile uint32 tim;

/*!
*/
void BlinkLEDs(int cnt) {
  int t;

  for (t = 0; t < cnt; t++) {
    for (tim = 0; tim < 500000; tim++)
      ;

    LedOn();

    for (tim = 0; tim < 500000; tim++)
      ;

    LedOff();
  }
}

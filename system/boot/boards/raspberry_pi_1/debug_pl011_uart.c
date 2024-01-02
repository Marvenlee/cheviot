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

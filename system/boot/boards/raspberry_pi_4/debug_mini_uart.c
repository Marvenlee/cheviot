#include "dbg.h"
#include "globals.h"
#include "string.h"
#include "types.h"
#include <stdarg.h>
#include <sys/syscalls.h>
#include "arm.h"



static volatile struct bcm2711_gpio_registers *gpio_regs;
static volatile struct bcm2711_aux_registers *aux_regs;


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


/* @brief   Initialise debug UART
 */
void dbg_init(void)
{
  BlinkLEDs(10);
  configure_uart();
}



/*
 *
 */
void configure_uart(void)
{
  dmb();

  gpio_regs = (struct bcm2711_gpio_registers *) GPIO_BASE;
  aux_regs = (struct bcm2711_aux_registers *) AUX_BASE;
  
  aux_regs->enables = 1;
  aux_regs->mu_ier_reg = 0;
  aux_regs->mu_cntl_reg = 0;
  aux_regs->mu_lcr_reg = 3;
  aux_regs->mu_mcr_reg = 0;
  aux_regs->mu_ier_reg = 0;
  aux_regs->mu_iir_reg = 0xC6;
  aux_regs->mu_baud_reg = AUX_MU_BAUD(115200)); 

  dmb();

  // select function 0 and disable pull up/down for pins 14, 15
//  configure_gpio(14, FN0, OFF);
//  configure_gpio(15, FN0, OFF);

  gpio_useAsAlt5(14);
  gpio_useAsAlt5(15);

  dmb();

  aux_regs->mu_cntl_reg = 3; //enable RX/TX

  dmb();
}


/* @brief   Configure GPIO registers
 */
void configure_gpio(uint32_t pin, enum FSel fn, enum PullUpDown action)
{    
    dmb();
    
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


/* @brief   Print panic message and halt boot process
 */
void KPanic(char *string)
{
  if (string != NULL) {
    KLog("%s", string);
  }

  while (1) {
  }
}

void KLog(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);

  Vsnprintf(&klog_entry[0], KLOG_WIDTH, format, ap);
  KPrintString(&klog_entry[0]);

  va_end(ap);
}


void KPrintString(char *s)
{  
  while (*s != '\0') {
    while (uart_write_ready() == false) {
    }
            
    uart_write_byte(*s);
    s++;
  }
  
  while (uart_write_ready() == false) {
  }
          
  uart_write_byte('\n');
}


/* @brief   Blink LEDs continuously,
 */
void BlinkError(void)
{
  static volatile int c;

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
void BlinkLEDs(int cnt)
{
  int t;
  static volatile int tim;

  for (t = 0; t < cnt; t++) {
    for (tim = 0; tim < 500000; tim++)
      ;

    LedOn();

    for (tim = 0; tim < 500000; tim++)
      ;

    LedOff();
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


bool uart_read_ready(void)
{ 
  volatile uint8_t val
  
  val = aux_regs->mu_lsr_reg;  
  return (val & 0x01) ? true: false;

}


bool uart_write_ready(void)
{
  volatile uint8_t val
  
  val = aux_regs->mu_lsr_reg;  
  return (val & 0x20) ? true: false;
}


char uart_read_byte(void)
{
  return (char)(aux_regs->mu_io_reg);
}


void uart_write_byte(char ch)
{
    aux_regs->mu_ioreg = (uint32_t)ch;
}


void set_gpio(uint32_t pin, bool state)
{
  if (state) {
    gpio_regs->set[pin / 32] = 1U << (pin % 32);
  } else {
    gpio_regs->clr[pin / 32] = 1U << (pin % 32);
  }
}


void io_delay(uint32_t cycles) 
{
  while(cycles-- > 0) {
    isb();
  }
}



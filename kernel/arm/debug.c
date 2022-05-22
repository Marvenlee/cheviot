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

/*
 * Debugging functions and Debug() system call.
 */

#define KDEBUG

#include <kernel/arm/arm.h>
#include <kernel/arm/globals.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <stdarg.h>

#ifdef KDEBUG


// Constants
#define KLOG_WIDTH 256


// Variables
static char klog_entry[KLOG_WIDTH];
bool processes_initialized = false;
bool debug_init = false;

// Prototypes
static void KPrintString(char *s);

#endif

/*
 *
 */

#ifdef KDEBUG

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
    // uart_regs->ctrl = CR_UARTEN | CR_RXE;

    uart_regs->ctrl = CR_UARTEN | CR_TXW | CR_RXE;

    dmb();
}
#endif

/* @brief Perform initialization of the kernel logger
 */
void InitDebug(void) {
#ifdef KDEBUG
//  configure_uart();
  debug_init = true;
#endif  
}

/* @brief Notify the kernel logger that processes are now running.
 *
 * Logging output is prefixed with the process ID of the caller 
 */
void ProcessesInitialized(void) {
#ifdef KDEBUG
  processes_initialized = true;
#endif
}

/*
 * ystem call allowing applications to print to serial without opening serial port.
 */
SYSCALL void SysDebug(char *s) {
#ifdef KDEBUG
  char buf[128];

  CopyInString (buf, s, sizeof buf);
  buf[sizeof buf - 1] = '\0';

  Info("PID %4d: %s:", GetPid(), &buf[0]);
#endif
}

/* @brief backend of kernel logger.
 *
 * Used by the macros KPRINTF, KLOG, KASSERT and KPANIC to
 * print with printf formatting to the kernel's debug log buffer.
 * The buffer is a fixed size circular buffer, Once it is full
 * the oldest entry is overwritten with the newest entry.
 *
 * Cannot be used during interrupt handlers or with interrupts
 * disabled s the dbg_slock must be acquired.  Same applies
 * to KPRINTF, KLOG, KASSERT and KPANIC.
 */

#ifdef KDEBUG
void DoLog(const char *format, ...) {
  va_list ap;

  va_start(ap, format);

  if (processes_initialized) {
    Snprintf(&klog_entry[0], 5, "%4d:", GetPid());
    Vsnprintf(&klog_entry[5], KLOG_WIDTH - 5, format, ap);
  } else {
    Vsnprintf(&klog_entry[0], KLOG_WIDTH, format, ap);
  }

  KPrintString(&klog_entry[0]);

  va_end(ap);
}

/*
 */
void MemDump(void *_addr, size_t sz) {
  char line[64];
  char tmp[4];
  uint8_t *addr = _addr;
  
  Info("MemDump Addr:%08x, sz:%d", (vm_addr)addr, sz);
  
  while (sz > 0) {
    line[0] = '\0';
    for (int x = 0; sz > 0 && x < 16; x++) {
      Snprintf(tmp, sizeof tmp, "%02x", (uint8_t)*addr);
      StrLCat(line, tmp, sizeof line); 
      addr++;
      sz --;
    }    

    Info ("%s", line);
  } 
}

/*
 */
int MemCmp(void *m1, void *m2, size_t sz) {
  uint8_t *a, *b;
  a = m1;
  b = m2;

  for (int t = 0; t < sz; t++) {
    if (*a != *b) {
      Error("!!!!!!!!!!!!!!! Mem Fail %08x %08x !!!!!!!!!!!!!!!!!!!!!",
            (vm_addr)a, (vm_addr)b);
      return -1;
    }
    a++;
    b++;
  }

  return 0;
}

#endif

/*
 * KernelPanic();
 */

void PrintKernelPanic(char *format, ...) {
  va_list ap;

  DisableInterrupts();
  
  va_start(ap, format);

#ifdef KDEBUG
  Vsnprintf(&klog_entry[0], KLOG_WIDTH, format, ap);
  KPrintString(&klog_entry[0]);
  KPrintString("### Kernel Panic ###");
#endif

  va_end(ap);  
  
  while(1);
}


#ifdef KDEBUG
static void KPrintString(char *s) {  

  if (debug_init == false) {
    return;
  }
  
  while (*s != '\0') {
    while (uart_regs->flags & FR_BUSY);        
    uart_regs->data = *s++;
  }
  
  while (uart_regs->flags & FR_BUSY);        
  uart_regs->data = '\n';
}
#endif


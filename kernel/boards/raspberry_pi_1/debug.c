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

#include <kernel/board/arm.h>
#include <kernel/board/globals.h>
#include <kernel/board/gpio.h>
#include <kernel/board/aux_uart.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/filesystem.h>
#include <stdarg.h>

// Constants
#define KLOG_WIDTH 256


// Variables
static char klog_entry[KLOG_WIDTH];
bool processes_initialized = false;
bool debug_initialized = false;

// Prototypes
static void KPrintString(char *s);


/* @brief Perform initialization of the kernel logger
 */
void InitDebug(void)
{
  aux_uart_init();
  debug_initialized = true;
}


/* @brief Notify the kernel logger that processes are now running.
 *
 * Logging output is prefixed with the process ID of the caller 
 */
void ProcessesInitialized(void)
{
  processes_initialized = true;
}


/*
 * ystem call allowing applications to print to serial without opening serial port.
 */
void sys_debug(char *s)
{
  char buf[128];

  CopyInString (buf, s, sizeof buf);
  buf[sizeof buf - 1] = '\0';

  Info("PID %4d: %s:", GetPid(), &buf[0]);
}


/* @brief   Debugging system call to test we can pass 6 arguments
 */
void sys_debug_sixargs(int a, int b, int c, int d, int e, int f, int dummy1, int dummy2) {

  Info("sys_debug_sixargs(%d %d %d %d %d %d : %08x, %08x)", a,b,c,d,e,f, dummy1, dummy2);
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

void DoLog(const char *format, ...)
{
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
 * KernelPanic();
 */
void PrintKernelPanic(char *format, ...) {
  va_list ap;

  DisableInterrupts();
  
  va_start(ap, format);

  Vsnprintf(&klog_entry[0], KLOG_WIDTH, format, ap);
  KPrintString(&klog_entry[0]);
  KPrintString("### Kernel Panic ###");

  va_end(ap);  
  
  while(1);
}


void KPrintString(char *string)
{
  char *ch = string;

  if (debug_initialized) {
    while (*ch != '\0') {
      aux_uart_write_byte(*ch++);
    }
    
    aux_uart_write_byte('\n');
  }
  
  // TODO:  Switch to writing to syslog inode once console driver is initialized.
}


/*
 *
 */
void PrintUserContext(struct UserContext *uc)
{
  DoLog("pc = %08x,   sp = %08x", uc->pc, uc->sp);
  DoLog("lr = %08x, cpsr = %08x", uc->lr, uc->cpsr);
  DoLog("r0 = %08x,   r1 = %08x", uc->r0, uc->r1);
  DoLog("r2 = %08x,   r3 = %08x", uc->r2, uc->r3);
  DoLog("r4 = %08x,   r5 = %08x", uc->r4, uc->r5);
  DoLog("r6 = %08x,   r7 = %08x", uc->r6, uc->r7);
  DoLog("r8 = %08x,   r9 = %08x", uc->r8, uc->r9);
  DoLog("r10 = %08x,  r11 = %08x   r12 = %08x", uc->r10, uc->r11, uc->r12);
}



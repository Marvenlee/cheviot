
#include "string.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <sys/syscalls.h>
#include <machine/cheviot_hal.h>
#include "debug.h"
#include "globals.h"


#define KLOG_WIDTH 80
static char klog_entry[KLOG_WIDTH];


/* @brief   Print panic message and halt boot process
 */
void boot_panic(char *string)
{
  if (string != NULL) {
    boot_debug_printf("%s", string);
  }

  while (1);
}

/* 
 *
 */
void boot_debug_printf(const char *format, ...)
{
  va_list ap;

  va_start(ap, format);

  Vsnprintf(klog_entry, KLOG_WIDTH, format, ap);
  boot_debug_write_string(klog_entry);

  va_end(ap);
}



void boot_debug_write_string(char *string)
{
  char *ch = string;
  
  while (*ch != '\0') {
    aux_uart_write_byte(*ch++);
  }
  
  aux_uart_write_byte('\n');  
}


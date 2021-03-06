/*
 * Copyright 2019  Marven Gilhespie
 *
 * Licensed under the Apache License, segment_id 2.0 (the "License");
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

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "serial.h"
#include "sys/debug.h"
#include <sys/syscalls.h>

/*
 *
 */
void init (int argc, char *argv[]) {

	termios.c_iflag = ICRNL;		/* Input */
	termios.c_oflag = ONLCR;		/* Output */
	termios.c_cflag = CS8;		/* Control */
	termios.c_lflag = ECHO | ECHONL | ICANON; /* Local */
	
	for (int t=0; t < NCCS; t++) {
		termios.c_cc[t] = 0;
	}
	
	termios.c_cc[VEOF]   = 0x04;	
	termios.c_cc[VEOL]   = '\n';
	termios.c_cc[VEOL2]  = '\r';
	termios.c_cc[VERASE] = 0x7f;
	termios.c_cc[VKILL]  = 0x40;

  termios.c_ispeed = 115200;
  termios.c_ospeed = 115200;
  
  tx_sz = 0;
  rx_sz = 0;
  tx_free_sz = sizeof tx_buf;
  rx_free_sz = sizeof rx_buf;

  taskcreate(reader_task, NULL, 4096);
  taskcreate(writer_task, NULL, 4096);
  taskcreate(uart_tx_task, NULL, 4096);
  taskcreate(uart_rx_task, NULL, 4096);

  if (process_args(argc, argv) != 0) {
    exit(-1);
  }
  
  // Align UART_BASE_PA down to 4k
  uart = (struct bcm2835_uart_registers *) VirtualAllocPhys((void *)0x50000000, 4096, PROT_READ | PROT_WRITE | CACHE_UNCACHEABLE, (void *)(BCM2708_PERI_BASE + UART_BASE_PA));
  
  if (uart == NULL) {
    exit(-1);
  }  
  
  // Align UART_BASE_PA down to 4k
  gpio = (struct bcm2835_gpio_registers *) VirtualAllocPhys((void *)0x50010000, 4096, PROT_READ | PROT_WRITE | CACHE_UNCACHEABLE, (void *)(BCM2708_PERI_BASE + GPIO_BASE_PA));
  
  if (gpio == NULL) {
    exit(-1);
  }
  
  configure_uart();
  
  if (mount_device() < 0) {
    exit(-1);
  }  
}

/*
 *
 */
int process_args(int argc, char *argv[]) {
  int c;

  // -u default user-id
  // -g default gid
  // -m default mod bits
  // -D debug level ?
  // mount path (default arg)
  
  if (argc <= 1) {
    return -1;
  }
  
  for (int t=0; t<argc; t++) {
  }

  while ((c = getopt(argc, argv, "u:g:m:")) != -1) {
    switch (c) {
    case 'u':
      config.uid = atoi(optarg);
      break;

    case 'g':
      config.gid = atoi(optarg);
      break;

    case 'm':
      config.mode = atoi(optarg);
      break;

    case 'b':
      config.baud = atoi(optarg);
      break;
    
    case 's':
      config.stop_bits = atoi(optarg);
      if (config.stop_bits < 0 || config.stop_bits > 2) {
        exit(-1);
      }
      break;
    
    case 'p':
      config.parity = true;
      break;  
      
    case 'f':
      if (strcmp(optarg, "hard") == 0) {
        config.flow_control = FLOW_CONTROL_HW;
      } else if (strcmp(optarg, "none") == 0) {
        config.flow_control = FLOW_CONTROL_HW;
      } else {
        exit(-1);
      }
      
      break;
      
    default:
      break;

    }
  }

  if (optind >= argc) {
    return -1;
  }

  strncpy(config.pathname, argv[optind], sizeof config.pathname);
  return 0;
}

/*
 *
 */
int mount_device(void) {
  struct stat stat;

  stat.st_dev = 0; // Get from config, or returned by Mount() (sb index?)
  stat.st_ino = 0;
  stat.st_mode = 0777 | _IFCHR;

  // default to read/write of device-driver uid/gid.
  stat.st_uid = 0;   // default device driver uid
  stat.st_gid = 0;   // default gid
  stat.st_blksize = 0;
  stat.st_size = 0;

  stat.st_blocks = 0;

  fd = Mount(config.pathname, 0, &stat);
  
  if (fd < 0) {
    exit (-1);
  }
  
  return fd;
}

/*
 *
 */
void configure_gpio(uint32_t pin, enum FSel fn, enum PullUpDown action) {
    dmb();
    
    // set pull up down
    // ----------------
    
    // set action & delay for 150 cycles.
    volatile uint32_t *pud = &gpio->pud;
    *pud = action;
    io_delay(1000);

    // trigger action & delay for 150 cycles.
    volatile uint32_t *clock = &gpio->pud_clk[pin / 32];
    *clock = (1 << (pin % 32));
    io_delay(1000);
    
    // clear action
    *pud = OFF;
	
    // remove clock
    *clock = 0;

    // set function
    // ------------
    volatile uint32_t *fsel = &gpio->fsel[pin / 10];
    uint32_t shift = (pin % 10) * 3;
    uint32_t mask = ~(7U << shift);
    *fsel = (*fsel & mask) | (fn << shift);
    
    dmb();
}

/*
 *
 */
void set_gpio(uint32_t pin, bool state)
{
    if (state) {
      gpio->set[pin / 32] = 1U << (pin % 32);
    } else {
      gpio->clr[pin / 32] = 1U << (pin % 32);
    }
}

/*
 *
 */
void configure_uart(void)
{
    dmb();
  
    // wait for end of transmission
    // while(uart->flags & FR_BUSY) { }

    // Disable UART0
    uart->ctrl = 0;
    io_delay(100);


    // flush transmit FIFO
    uart->lcrh &= ~LCRH_FEN;	
	
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
    
    uart->ibrd = divider;
    uart->fbrd = fraction;

    // Enable FIFO & 8 bit data transmission (1 stop bit, no parity)
    uart->lcrh = LCRH_FEN | LCRH_WLEN8;
    uart->lcrh = LCRH_WLEN8;

    // set FIFO interrupts to fire at half full
    uart->ifls = IFSL_RX_1_2 | IFSL_TX_1_2;
    
    // Clear pending interrupts.
    uart->icr = INT_ALL;

    // Mask all interrupts.
    uart->imsc = INT_ALL;
    
    // Enable UART0, receive & transfer part of UART.
    // uart->ctrl = CR_UARTEN | CR_RXE;

    uart->ctrl = CR_UARTEN | CR_TXW | CR_RXE;

    dmb();

    interrupt_fd = CreateInterrupt(SERIAL_IRQ, &interrupt_handler);
  
    if (interrupt_fd < 0) {
      exit(-1);
    }
}

/*
 *
 */
void io_delay(uint32_t cycles) 
{
    while(cycles-- > 0) isb();
}


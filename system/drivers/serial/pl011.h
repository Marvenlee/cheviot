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

#ifndef SERIAL_H
#define SERIAL_H

//#define NDEBUG

#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include <sys/lists.h>
#include <sys/fsreq.h>
#include <sys/termios.h>
#include <sys/interrupts.h>
#include <sys/syscalls.h>
#include <sys/syslimits.h>
#include <task.h>



/*
 */
#define NMSG_BACKLOG 			 3		// 1 read, 1 write, 1 ioctl/termios/synchronous function
#define POLL_TIMEOUT 	100000

/*
 * Configuration settings
 */
struct Config
{
  char pathname[PATH_MAX + 1];
  uid_t uid;
  gid_t gid;
  mode_t mode;
	dev_t dev;
	
  int baud;
  int stop_bits;
  bool hw_flow_control;
  bool parity;
};


// prototypes
void cmd_isatty(msgid_t msgid, struct fsreq *req);
void cmd_read(msgid_t msgid, struct fsreq *req);
void cmd_write(msgid_t msgid, struct fsreq *req);
void cmd_tcsetattr(msgid_t msgid, struct fsreq *req);
void cmd_tcgetattr(msgid_t msgid, struct fsreq *req);

void init(int argc, char *argv[]);
int process_args(int argc, char *argv[]);
int mount_device(void);

void reader_task(void *arg);
void writer_task(void *arg);
void uart_tx_task(void *arg);
void uart_rx_task(void *arg);

void line_discipline(uint8_t ch);
int get_line_length(void);
void echo(uint8_t ch);

void Delay(uint32_t cycles);

void isb(void);
void dsb(void);
void dmb(void);

void input_processing(uint8_t ch);

int pl011_uart_configure(void);
bool pl011_uart_rx_ready(void);
bool pl011_uart_tx_ready(void);
char pl011_uart_read_byte(void);
void pl011_uart_write_byte(char ch);
void pl011_uart_interrupt_isr(int irq, struct InterruptAPI *api);
void pl011_uart_interrupt_bottom_half(void);


#endif


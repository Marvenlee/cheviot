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
#include <sys/syscalls.h>
#include <unistd.h>
#include <sys/fsreq.h>
#include <sys/termios.h>
#include "pl011.h"


int portid;
int kq;
int interrupt_fd;

int sid = 0;

int tx_head;
int tx_sz;
int tx_free_head;
int tx_free_sz;
uint8_t tx_buf[4096];

int rx_head;
int rx_sz;
int rx_free_head;
int rx_free_sz;
uint8_t rx_buf[4096];

int line_cnt;
int line_end;

bool write_pending;
bool read_pending;
int read_msgid;
int write_msgid;

Rendez tx_rendez;
Rendez rx_rendez;

Rendez tx_free_rendez;
Rendez rx_data_rendez;

Rendez write_cmd_rendez;
Rendez read_cmd_rendez;

struct fsreq read_fsreq;
struct fsreq write_fsreq;


struct termios termios;


#if defined(BOARD_RASPBERRY_PI_1)
volatile struct bcm2835_uart_registers *uart;
volatile struct bcm2835_gpio_registers *gpio;
#elif defined(BOARD_RASPBERRY_PI_4)
volatile struct bcm2711_aux_registers *aux;
volatile struct bcm2711_gpio_registers *gpio;
#endif



struct Config config;



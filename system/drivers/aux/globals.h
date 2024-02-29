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

#ifndef SERIAL_GLOBALS_H
#define SERIAL_GLOBALS_H

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/syscalls.h>
#include <unistd.h>
#include <sys/fsreq.h>
#include <poll.h>
#include <sys/termios.h>
#include "aux_uart.h"


extern int portid;
extern int kq;
extern int interrupt_fd;
extern int sid;

extern struct kevent ev;
extern struct kevent setev;


extern uint32_t tx_head;
extern uint32_t tx_sz;
extern uint32_t tx_free_head;
extern uint32_t tx_free_sz;
extern uint8_t tx_buf[4096];

extern uint32_t rx_head;
extern size_t rx_sz;
extern uint32_t rx_free_head;
extern size_t rx_free_sz;
extern uint8_t rx_buf[4096];

extern uint32_t line_cnt;
extern uint32_t line_end;

extern bool write_pending;
extern bool read_pending;


extern msgid_t read_msgid;
extern msgid_t write_msgid;

extern Rendez tx_rendez;
extern Rendez rx_rendez;

extern Rendez tx_free_rendez;
extern Rendez rx_data_rendez;

extern Rendez write_cmd_rendez;
extern Rendez read_cmd_rendez;

extern struct fsreq read_fsreq;
extern struct fsreq write_fsreq;

extern struct termios termios;

extern struct Config config;

#endif


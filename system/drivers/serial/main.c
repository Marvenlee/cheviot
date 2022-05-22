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
#include <poll.h>
#include "serial.h"
#include <sys/debug.h>
#include <sys/lists.h>
#include <task.h>


int LineLength(void);

/*
 * main
 */
void taskmain(int argc, char *argv[]) {
  struct fsreq req;
  int sc;
  int msgid;
  struct pollfd pfd[2];
  uint32_t mis;

  Debug ("**** serial.exe ****");

  init(argc, argv);

  while (1) {
    pfd[0].fd = fd;
    pfd[0].events = POLLIN;
    pfd[0].revents = 0;
    pfd[1].fd = interrupt_fd;
    pfd[1].events = POLLPRI;
    pfd[1].revents = 0;
    
    // TODO:  POLL_TIMEOUT to become inter-char timeout if input == RAW and VTIME != 0

    sc = poll(pfd, 2, POLL_TIMEOUT);
    
    if (pfd[0].revents & POLLIN) {        
      while ((sc = ReceiveMsg(fd, &msgid, &req, sizeof req)) == sizeof req) {
        switch (req.cmd) {
          case CMD_READ:
            cmd_read(msgid, &req);
            break;

          case CMD_WRITE:
            cmd_write(msgid, &req);
            break;

          case CMD_ISATTY:
            cmd_isatty(msgid, &req);
            break;

          case CMD_TCSETATTR:
            cmd_tcsetattr(msgid, &req);
            break;

          case CMD_TCGETATTR:
            cmd_tcgetattr(msgid, &req);
            break;

          default:
            exit(-1);
        }
      }
      
      if (sc != 0) {
        exit(-1);
      }
    }

    if (pfd[1].revents & POLLPRI) {
        // Read interrupt status register and invoke deferred-procedure-call tasklets

        dmb();    
//      mis = uart->mis;      
        dmb();    

//      if (mis & (INT_RXR | INT_RTR)) {
        taskwakeupall(&rx_rendez);
//      }

//      if (mis & INT_TXR) {
        taskwakeupall(&tx_rendez);
//      }      

        interrupt_clear();
    }
        
    while (taskyield() != 0);
  }

  exit(0);
}

/*
 *
 */
void cmd_isatty (int msgid, struct fsreq *fsreq)
{
  struct fsreply reply;
  
  reply.args.isatty.status = 0;
	SeekMsg(fd, msgid, sizeof *fsreq);
	WriteMsg(fd, msgid, &reply, sizeof reply);
	ReplyMsg (fd, msgid, 0);
}

/*
 *
 */
void cmd_tcgetattr (int msgid, struct fsreq *fsreq)
{	
  struct fsreply reply;
  
  SeekMsg(fd, msgid, sizeof *fsreq + sizeof reply);		
  WriteMsg(fd, msgid, &termios, sizeof termios);

  reply.args.tcgetattr.status = 0;
	SeekMsg(fd, msgid, sizeof *fsreq);
	WriteMsg(fd, msgid, &reply, sizeof reply);
	ReplyMsg (fd, msgid, 0);
}

/*
 * FIX: add actions to specify when/what should be modified/flushed.
 */ 
void cmd_tcsetattr (int msgid, struct fsreq *fsreq)
{
  struct fsreply reply;
  
  SeekMsg(fd, msgid, sizeof *fsreq + sizeof reply);		
  ReadMsg(fd, msgid, &termios, sizeof termios);

  // Perhaps make sure whole termios is written.

  // TODO: Flush any buffers, change stream mode to canonical etc
  
  reply.args.tcsetattr.status = 0;
	SeekMsg(fd, msgid, sizeof *fsreq);
	WriteMsg(fd, msgid, &reply, sizeof reply);
	ReplyMsg (fd, msgid, 0);
}

/*
 * UID belongs to sending thread (for char devices)
 * Can only be 1 thread doing read at a time.
 *
 * Only 1 reader and only 1 writer and only 1 tc/isatty cmd
 */
void cmd_read(int msgid, struct fsreq *req) {

  read_pending = true;
  read_msgid = msgid;
  memcpy (&read_fsreq, req, sizeof read_fsreq);
  taskwakeup (&read_cmd_rendez);
}

/*
 * 
 */
void cmd_write(int msgid, struct fsreq *req) {

  write_pending = true;
  write_msgid = msgid;
  memcpy (&write_fsreq, req, sizeof write_fsreq);
  taskwakeup(&write_cmd_rendez);
}

/*
 *
 */
void reader_task (void *arg)
{
  struct fsreply reply;
  ssize_t nbytes_read;
  size_t line_length;
  size_t remaining;
  uint8_t *buf;
  size_t sz;
  
  while (1) {

    while (read_pending == false) {
      tasksleep (&read_cmd_rendez);
    }

    read_pending = true;
    
    while (rx_sz == 0) {
      tasksleep(&rx_data_rendez);
    }

    if (termios.c_lflag & ICANON) {
      while(line_cnt == 0) {
        tasksleep(&rx_data_rendez);
      }
    
      // Could we remove line_length calculation each time ?
      line_length = LineLength();
      remaining = (line_length < read_fsreq.args.read.sz) ? line_length : read_fsreq.args.read.sz;
    }

    SeekMsg(fd, read_msgid, sizeof (struct fsreq) + sizeof reply);
         
    nbytes_read = 0;
    
    if (rx_head + remaining > sizeof rx_buf) {
      sz = sizeof rx_buf - rx_head;
      buf = &rx_buf[rx_head]; 
      
      WriteMsg(fd, read_msgid, buf, sz);
      nbytes_read += sz;
    
      sz = remaining -= sz;
      buf = &rx_buf[0];
    } else {
      sz = remaining;
      buf = &rx_buf[rx_head];
    }
    
    WriteMsg(fd, read_msgid, buf, sz);
    nbytes_read += sz;

    rx_head = (rx_head + nbytes_read) % sizeof rx_buf;
    rx_free_sz += nbytes_read;
    rx_sz -= nbytes_read;
    
    taskwakeupall(&tx_rendez);

    if (termios.c_lflag & ICANON) {
      if (nbytes_read == line_length) {
        line_cnt--;
      }
    }
          
    taskwakeupall(&rx_rendez);
    
    
    reply.cmd = CMD_READ;
    reply.args.read.nbytes_read = nbytes_read;          
    SeekMsg(fd, read_msgid, sizeof (struct fsreq));
    WriteMsg(fd, read_msgid, &reply, sizeof reply);

    ReplyMsg(fd, read_msgid, 0);  
    read_pending = false;
  }
}

/*
 *
 */
void writer_task (void *arg)
{
  struct fsreply reply;
  ssize_t nbytes_written;
  size_t remaining;
  uint8_t *buf;
  size_t sz;
  
  while (1) {
    while (write_pending == false) {
      tasksleep (&write_cmd_rendez);
    }
    
//    KLog ("write pending awakened");
    
    write_pending = true;

    if (tx_free_sz < 0) {
      exit(7);
    }
    
    while (tx_free_sz == 0) {
      tasksleep(&tx_free_rendez);
    }

    SeekMsg(fd, write_msgid, sizeof (struct fsreq) + sizeof reply);
    
    remaining = (tx_free_sz < write_fsreq.args.write.sz) ? tx_free_sz : write_fsreq.args.write.sz;
    nbytes_written = 0;
        
    if (tx_free_head + remaining > sizeof tx_buf) {
      sz = sizeof tx_buf - tx_free_head;
      buf = &tx_buf[tx_free_head]; 
      
      ReadMsg(fd, write_msgid, buf, sz);

      nbytes_written += sz;      
      sz = remaining -= sz;
      buf = &tx_buf[0];
    } else {
      sz = remaining;
      buf = &tx_buf[tx_free_head];
    }


//    KLog ("Writer ReadMsg2: sz:%d", sz);
    
    if (sz > sizeof tx_buf) {
      exit (8);
    }
    
    ReadMsg(fd, write_msgid, buf, sz);
    nbytes_written += sz;

    tx_free_head = (tx_free_head + nbytes_written) % sizeof tx_buf;
    tx_free_sz -= nbytes_written;
    tx_sz += nbytes_written;
        
    reply.cmd = CMD_WRITE;
    reply.args.write.nbytes_written = nbytes_written;          
    SeekMsg(fd, write_msgid, sizeof (struct fsreq));
    WriteMsg(fd, write_msgid, &reply, sizeof reply);
    ReplyMsg(fd, write_msgid, 0);  
    write_pending = false;

    taskwakeupall(&tx_rendez);
  }
}

/*
 * Effectively a deferred procedure call for interrupts running on task state
 */
void uart_tx_task(void *arg)
{
  
  while (1) {  
    dmb();

    while (tx_sz == 0 || (uart->flags & FR_TXFF)) {
      tasksleep(&tx_rendez);
    }
  
    dmb();

    if (tx_sz + tx_free_sz > sizeof tx_buf) {
      exit(8);
    }
    
    while (tx_sz > 0 /*&& (uart->flags & FR_TXFF) == 0*/) {
        
        while (uart->flags & FR_BUSY);
        
        uart->data = tx_buf[tx_head];
        tx_head = (tx_head + 1) % sizeof tx_buf;
        tx_sz--;
        tx_free_sz++;
    }
    
    dmb();    
  
    if (tx_free_sz > 0) {
      // PollNotify(fd, INO_NR, POLLIN, POLLIN);
      taskwakeupall(&tx_free_rendez);
    }   
  }
}

/*
 * Effectively a deferred procedure call for interrupts running on task state
 */
void uart_rx_task(void *arg)
{
  uint32_t flags;
  uint8_t ch;
  
  while(1)
  {
    dmb();

    while (rx_free_sz == 0 || (uart->flags & FR_RXFE)) {
      tasksleep (&rx_rendez);
    }

    dmb();
    
    while(rx_free_sz > 0 && (uart->flags & FR_RXFE) == 0) {      
      ch = uart->data;
      line_discipline(ch);         
    }  

    dmb();
    
    if (termios.c_lflag & ICANON) {
      if (line_cnt > 0) {
        taskwakeupall(&rx_data_rendez);
      }
    }
    else if (rx_sz > 0) {
        taskwakeupall(&rx_data_rendez);
        // PollNotify(fd, INO_NR, POLLIN, ~POLLIN);
    }
  }
}

/*
 *
 */
int add_to_rx_queue(uint8_t ch)
{
  if (rx_free_sz > 1) {
    rx_buf[rx_free_head] = ch;
    rx_sz++;
    rx_free_head = (rx_free_head + 1) % sizeof rx_buf;
    rx_free_sz--;
  }
  
  return 0;
}

/*
 *
 */
void line_discipline(uint8_t ch)
{
  int last_char;  

  if ((ch == termios.c_cc[VEOL] || ch == termios.c_cc[VEOL2]) && (termios.c_lflag & ECHONL)) {        
      echo(ch);    
  } else {       
    if (!(ch == termios.c_cc[VEOL] || ch == termios.c_cc[VEOL2]) && (termios.c_lflag & ECHO)) {
      echo(ch);
    }
  }
  
  if (termios.c_lflag & ICANON) {
    if (ch == termios.c_cc[VEOL]) {
      add_to_rx_queue('\n');
      line_cnt++;
    } else if (ch == termios.c_cc[VEOL2]) {
      add_to_rx_queue('\n');
      line_cnt++;
    } else if (ch == termios.c_cc[VERASE]) {
      if (rx_sz > 0) {
        last_char = rx_buf[rx_sz-1];            
        if (last_char != termios.c_cc[VEOL] && last_char != termios.c_cc[VEOL2]) {
          rx_sz --;
          rx_free_head = (rx_free_head - 1) % sizeof rx_buf;
          rx_free_sz++;
        }
      }
    } else {
      
      if (rx_free_sz > 2) {
        add_to_rx_queue(ch);
      }
    
    }
  } else { 
    add_to_rx_queue(ch);
  }  
}

/*
 *
 */
int LineLength(void)
{  
  for (int t=0; t < rx_sz; t++) {    
    if (rx_buf[(rx_head + t) % sizeof rx_buf] == termios.c_cc[VEOL] ||
        rx_buf[(rx_head + t) % sizeof rx_buf] == termios.c_cc[VEOL2]) {
      return t+1;
    }
  }
  
  // Shouldn't get here
  return rx_sz;
}

/*
 *
 */
void echo(uint8_t ch)
{
    while (tx_free_sz == 0) {
      tasksleep(&tx_rendez);
    }

    tx_buf[tx_free_head] = ch;
    tx_free_head = (tx_free_head + 1) % sizeof rx_buf;
    tx_free_sz--;
    tx_sz++;
    
    taskwakeupall(&tx_rendez);
}

/*
 *
 */
void interrupt_handler(int irq, struct InterruptAPI *api)
{    
  dmb();
  api->MaskInterrupt(SERIAL_IRQ);
  api->PollNotifyFromISR(api, POLLPRI, POLLPRI);  
  dmb();
}

/*
 *
 */
void interrupt_clear(void)
{
  uart->rsrecr = 0;
  uart->icr = 0xffffffff;
  UnmaskInterrupt(SERIAL_IRQ);
}



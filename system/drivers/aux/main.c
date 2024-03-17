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
#include <unistd.h>
#include <poll.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/syscalls.h>
#include <sys/fsreq.h>
#include <sys/debug.h>
#include <sys/lists.h>
#include <sys/event.h>
#include <task.h>
#include "common.h"
#include "globals.h"


/* @brief   Main task of the serial driver for Pi's mini-uart
 *
 * Libtask coroutines are used for concurrency in the serial driver
 * We follow Djikstra's Secretaries and Directors pattern of 
 * Cooperating Sequential Processes.  We have the main task acting
 * as the secretary, waiting for external events such as messages
 * arriving on the message port, timers or when implemented, interrupts.
 * This then wakes up director tasks or sub-secretary tasks as needed.
 *
 * We perform a repeated taskyield at the end to let these other tasks
 * run. Once that returns 0, indicating no other tasks are in a runnable
 * state then this main task (secretary) goes back to waiting for
 * incoming events.
 *
 * For the Raspberry Pi 1 we use the pl011 uart.
 */



void taskmain(int argc, char *argv[])
{
  struct fsreq req;
  int sc;
  int nevents;
  msgid_t msgid;
  struct timespec timeout;

  log_info("aux: main");
   
  init(argc, argv);

  taskcreate(reader_task, NULL, 8092);
  taskcreate(writer_task, NULL, 8092);
  taskcreate(uart_tx_task, NULL, 8092);
  taskcreate(uart_rx_task, NULL, 8092);

  timeout.tv_sec = 0;
  timeout.tv_nsec = 50000000;
    
  EV_SET(&setev, portid, EVFILT_MSGPORT, EV_ADD | EV_ENABLE, 0, 0, 0); 
  // EV_SET(&setev[1], interrupt_fd, EVFILT_IRQ, EV_ADD | EV_ENABLE, 0, 0, 0); 
  kevent(kq, &setev, 1,  NULL, 0, NULL);

  while (1) {
    errno = 0;
    nevents = kevent(kq, NULL, 0, &ev, 1, &timeout);

    if (nevents == 1 && ev.ident == portid && ev.filter == EVFILT_MSGPORT) {
      while ((sc = getmsg(portid, &msgid, &req, sizeof req)) == sizeof req) {
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
            log_warn("aux: unknown command: %d", req.cmd);
            replymsg(portid, msgid, -ENOTSUP, NULL, 0);
            break;
        }
      }      
      
      if (sc != 0) {
        log_error("aux: getmsg sc=%d %s", sc, strerror(errno));
        exit(EXIT_FAILURE);
      }
    }

    // Read interrupt status register and wakeup appropriate rx and tx tasks
#if 0  
    if (nevents == 1 && ev.ident == interrupt_fd && ev.filter == EVFILT_IRQ)
#endif    
    {      
      aux_uart_interrupt_bottom_half();  
    }
    
    //  Yield until no other task is running.            
    while (taskyield() != 0);
  }

  exit(0);
}


/*
 *
 */
void cmd_isatty (msgid_t msgid, struct fsreq *fsreq)
{
  replymsg(portid, msgid, 1, NULL, 0);
}


/*
 *
 */
void cmd_tcgetattr(msgid_t msgid, struct fsreq *fsreq)
{	
  replymsg(portid, msgid, 0, &termios, sizeof termios);
}

/*
 * FIX: add actions to specify when/what should be modified/flushed.
 */ 
void cmd_tcsetattr(msgid_t msgid, struct fsreq *fsreq)
{
  readmsg(portid, msgid, &termios, sizeof termios, sizeof *fsreq);

  // TODO: Flush any buffers, change stream mode to canonical etc

	replymsg(portid, msgid, 0, NULL, 0);
}


/*
 * UID belongs to sending thread (for char devices)
 * Can only be 1 thread doing read at a time.
 *
 * Only 1 reader and only 1 writer and only 1 tc/isatty cmd
 */
void cmd_read(msgid_t msgid, struct fsreq *req)
{
  read_pending = true;
  read_msgid = msgid;
  memcpy (&read_fsreq, req, sizeof read_fsreq);
  
//  log_info("aux: read nbytes:%u", read_fsreq.args.read.sz);
  
  taskwakeup (&read_cmd_rendez);
}


/*
 * 
 */
void cmd_write(msgid_t msgid, struct fsreq *req)
{
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
  ssize_t nbytes_read;
  size_t line_length;
  size_t remaining;
  uint8_t *buf;
  size_t sz;

  while (1) {
    while (read_pending == false) {
      tasksleep (&read_cmd_rendez);
    }

    sz = 0;
    remaining = 0;
    buf = NULL;
    nbytes_read = 0;
    
    while (rx_sz == 0) {
      tasksleep(&rx_data_rendez);
    }

    if (termios.c_lflag & ICANON) {
      while(line_cnt == 0) {
        tasksleep(&rx_data_rendez);
      }
    
      // Could we remove line_length calculation each time ?
      line_length = get_line_length();
      remaining = (line_length < read_fsreq.args.read.sz) ? line_length : read_fsreq.args.read.sz;
    } else {
      remaining = (rx_sz < read_fsreq.args.read.sz) ? rx_sz : read_fsreq.args.read.sz;
    }
         
    
    if (rx_head + remaining > sizeof rx_buf) {
      sz = sizeof rx_buf - rx_head;
      buf = &rx_buf[rx_head]; 
      
      writemsg(portid, read_msgid, buf, sz, 0);

      nbytes_read = sz;
    
      sz = remaining - sz;
      buf = &rx_buf[0];
    } else {
	    nbytes_read = 0;
      sz = remaining;
      buf = &rx_buf[rx_head];
    }
        
    writemsg(portid, read_msgid, buf, sz, nbytes_read);

    nbytes_read += sz;

    rx_head = (rx_head + nbytes_read) % sizeof rx_buf;
    rx_free_sz += nbytes_read;
    rx_sz -= nbytes_read;
    
    if (termios.c_lflag & ICANON) {
      if (nbytes_read == line_length) {
        line_cnt--;
      }
    }
          
    taskwakeupall(&rx_rendez);
    
    replymsg(portid, read_msgid, nbytes_read, NULL, 0);

    read_msgid = -1;
    read_pending = false;
  }
}


/*
 *
 */
void writer_task (void *arg)
{
  ssize_t nbytes_written;
  size_t remaining;
  uint8_t *buf = NULL;
  size_t sz;
  
  while (1) {
    while (write_pending == false) {
      tasksleep (&write_cmd_rendez);
    }
    
    sz = 0;
    remaining = 0;
    buf = NULL;
    nbytes_written = 0;
    
    if (tx_free_sz < 0) {
      log_error("tx_free_sz = %d, < 0", tx_free_sz);
      exit(7);
    }
    
    while (tx_free_sz == 0) {
      tasksleep(&tx_free_rendez);
    }

    remaining = (tx_free_sz < write_fsreq.args.write.sz) ? tx_free_sz : write_fsreq.args.write.sz;
    nbytes_written = 0;
        
    if (tx_free_head + remaining > sizeof tx_buf) {
      sz = sizeof tx_buf - tx_free_head;
      buf = &tx_buf[tx_free_head]; 

      readmsg(portid, write_msgid, buf, sz, sizeof (struct fsreq));

      nbytes_written += sz;      
      sz = remaining - sz;
      buf = &tx_buf[0];
    } else {
      sz = remaining;
      buf = &tx_buf[tx_free_head];
    }

    if (sz > sizeof tx_buf) {
      log_error("write sz > tx_buf");
      exit (8);
    }
    
    if (sz == 0) {
      log_error("write sz == 0");
      exit (8);
    }
    
    readmsg(portid, write_msgid, buf, sz, sizeof (struct fsreq) + nbytes_written);
    nbytes_written += sz;

    tx_free_head = (tx_free_head + nbytes_written) % sizeof tx_buf;
    tx_free_sz -= nbytes_written;
    tx_sz += nbytes_written;

    replymsg(portid, write_msgid, nbytes_written, NULL, 0);
    
    write_msgid = -1;
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
    while (tx_sz == 0 || aux_uart_write_ready() == false) {
      tasksleep(&tx_rendez);
    }
  
    if (tx_sz + tx_free_sz > sizeof tx_buf) {
      exit(EXIT_FAILURE);
    }
    
    while (tx_sz > 0 /* && uart_tx_ready() == true */) {
        
        if (tx_head >= sizeof tx_buf) {
          exit(-1);
        }

        aux_uart_write_byte(tx_buf[tx_head]);
        tx_head = (tx_head + 1) % sizeof tx_buf;
        tx_sz--;
        tx_free_sz++;
    }

    if (tx_free_sz > 0) {
      // TODO: PollNotify(fd, INO_NR, POLLIN, POLLIN);
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
  int ready;
  
  while(1)
  {
    while (rx_free_sz == 0 || (ready = aux_uart_read_ready()) == false) {
      tasksleep (&rx_rendez);
    }

    while(rx_free_sz > 0 && aux_uart_read_ready() == true) {
      ch = aux_uart_read_byte();
      line_discipline(ch);         
    }  

    if (termios.c_lflag & ICANON) {
      if (line_cnt > 0) {
        taskwakeupall(&rx_data_rendez);
      }
    }
    else if (rx_sz > 0) {
        taskwakeupall(&rx_data_rendez);
        // TODO: PollNotify(fd, INO_NR, POLLIN, ~POLLIN);
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
      if (rx_free_sz > 2) {  // leave space for '\n'
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
int get_line_length(void)
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
    tx_free_head = (tx_free_head + 1) % sizeof tx_buf;
    tx_free_sz--;
    tx_sz++;
    
    taskwakeupall(&tx_rendez);
}





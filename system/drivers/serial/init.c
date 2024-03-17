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
#include <sys/debug.h>
#include <sys/syscalls.h>
#include <sys/event.h>
#include "pl011.h"
#include "globals.h"

/*
 *
 */
void init (int argc, char *argv[])
{
  log_info("aux uart - init");

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

  if (process_args(argc, argv) != 0) {
    exit(EXIT_FAILURE);
  }
    
  if (pl011_uart_configure() != 0) {
    log_error("aux uart initialization failed, exiting");
    exit(EXIT_FAILURE);
  }

  if (mount_device() < 0) {
    log_error("mount device failed, exiting");
    exit(EXIT_FAILURE);
  }

  kq = kqueue();
  
  if (kq < 0) {
    log_error("create kqueue for serial failed");
    exit(EXIT_FAILURE);
  }
}


/*
 *
 */
int process_args(int argc, char *argv[])
{
  int c;
  
  if (argc <= 1) {
    return -1;
  }
  
  config.uid = getuid();
  config.gid = getgid();
  config.dev = -1;
  config.mode = 0700;
  config.baud = 115200;
  config.parity = false;
  config.stop_bits = 1;
  config.hw_flow_control = false;
  
  while ((c = getopt(argc, argv, "u:g:m:d:b:s:pf")) != -1) {
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
		      
    case 'd':
      config.dev = atoi(optarg);
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
      config.hw_flow_control = true;      
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
int mount_device(void)
{
  struct stat mnt_stat;

  mnt_stat.st_dev = config.dev; // Get from config or env
  mnt_stat.st_ino = 0;
  mnt_stat.st_mode = S_IFCHR | (config.mode & 0777);

  // default to read/write of device-driver uid/gid.
  mnt_stat.st_uid = config.uid;
  mnt_stat.st_gid = config.gid;
  mnt_stat.st_blksize = 0;
  mnt_stat.st_size = 0;
  mnt_stat.st_blocks = 0;
  
  portid = createmsgport(config.pathname, 0, &mnt_stat, NMSG_BACKLOG);
  
  if (portid < 0) {
    return -1;
  }

  return 0;
}



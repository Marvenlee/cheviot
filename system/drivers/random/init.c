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
#include "trng.h"
#include "globals.h"
#include "random.h"


/*
 *
 */
void init (int argc, char *argv[])
{
  log_info("random: init");
		
  if (process_args(argc, argv) != 0) {
    exit(EXIT_FAILURE);
  }
    
  if (trng_hw_init() != 0) {
    log_error("random: initialization failed, exiting");
    exit(EXIT_FAILURE);
  }

  if (mount_device() < 0) {
    log_error("random: mount device failed, exiting");
    exit(EXIT_FAILURE);
  }

  kq = kqueue();
  
  if (kq < 0) {
    log_error("random: create kqueue for serial failed");
    exit(EXIT_FAILURE);
  }
}


/*
 *
 */
int process_args(int argc, char *argv[])
{
  int c;

  config.uid = 0;
  config.gid = 0;
  config.dev = -1;
  config.mode = 0777 | S_IFCHR;
  
  if (argc <= 1) {
    return -1;
  }
  
  while ((c = getopt(argc, argv, "u:g:m:d:")) != -1) {
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

  mnt_stat.st_dev = config.dev; // Get from config, or returned by Mount() (sb index?)
  mnt_stat.st_ino = 0;
  mnt_stat.st_mode = S_IFCHR | (config.mode & 0777);

  // default to read/write of device-driver uid/gid.
  mnt_stat.st_uid = config.uid;   // default device driver uid
  mnt_stat.st_gid = config.gid;   // default gid
  mnt_stat.st_blksize = 0;
  mnt_stat.st_size = 0;
  mnt_stat.st_blocks = 0;
  
  portid = createmsgport(config.pathname, 0, &mnt_stat, NMSG_BACKLOG);
  
  if (portid < 0) {
    return -1;
  }

  return 0;
}



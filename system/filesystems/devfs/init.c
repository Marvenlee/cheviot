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

#include "globals.h"
#include "devfs.h"
#include "sys/debug.h"
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
#include <sys/event.h>

/*
 *
 */
void init (int argc, char *argv[])
{
  log_info("devfs - init");

  if (init_devfs() != 0) {
    exit(-1);
  }
  
  if (mount_device() != 0) {
    exit(-1);
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
  config.mode = 0777;
  
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

  return 0;
}


/*
 *
 */
int init_devfs(void)
{
  devfs_inode_table[0].inode_nr = 0;
  devfs_inode_table[0].parent_inode_nr = 0;    
  devfs_inode_table[0].name[0] = '\0';

  for (int t=1; t< DEVFS_MAX_INODE; t++) {
    devfs_inode_table[t].inode_nr = t;
    devfs_inode_table[t].parent_inode_nr = 0;    
    devfs_inode_table[t].name[0] = '\0';
  }
  
  return 0;
}

/*
 *
 */
int mount_device(void)
{
  struct stat mnt_stat;

  mnt_stat.st_dev = config.dev;
  mnt_stat.st_ino = 0;
  mnt_stat.st_mode = S_IFDIR | (config.mode & 0777);
  mnt_stat.st_uid = config.uid;
  mnt_stat.st_gid = config.gid;
  mnt_stat.st_blksize = 512;
  mnt_stat.st_size = 0;
  mnt_stat.st_blocks = 0;
  
  portid = createmsgport("/dev", 0, &mnt_stat, NMSG_BACKLOG);
  
  if (portid == -1) {
    log_error("Failed to mount /dev\n");
    exit(-1);
  }

  kq = kqueue();
    
  if (kq == -1) {
    log_error("Failed to create kqueue for devfs\n");
    exit(-1);
  }
   
  return 0;
}





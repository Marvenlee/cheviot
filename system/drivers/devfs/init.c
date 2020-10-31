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


/*
 *
 */
void init (int argc, char *argv[]) {
  KLog ("devfs.exe init()");

  if (initDevfs() != 0) {
    exit(-1);
  }
  
  if (mountDevice() != 0) {
    exit(-1);
  }
}

/*
 *
 */
int processArgs(int argc, char *argv[]) {
  if (argc < 3) {
    return -1;
  }

  config.devfs_image = strtoul(argv[1], NULL, 0);
  config.devfs_image_size = strtoul(argv[2], NULL, 0);
  return 0;
}

/**
 *
 */
int initDevfs(void) {
  devfs_inode_table[0].inode_nr = 0;
  devfs_inode_table[0].parent_inode_nr = 0;    
  devfs_inode_table[0].name[0] = '\0';

  for (int t=1; t< MAX_INODE; t++) {
    devfs_inode_table[t].inode_nr = t;
    devfs_inode_table[t].parent_inode_nr = 0;    
    devfs_inode_table[t].name[0] = '\0';
  }
  
  return 0;
}

/*
 *
 */
int mountDevice(void) {
  struct stat stat;

  stat.st_dev = 0; // Get from config, or returned by Mount() (sb index?)
  stat.st_ino = 0;
  stat.st_mode = 0777 | _IFDIR; // default to read/write of device-driver uid/gid.
  stat.st_uid = 0;   // default device driver uid
  stat.st_gid = 0;   // default gid
  stat.st_blksize = 512;
  stat.st_size = 0;
  stat.st_blocks = 0;
  
  fd = Mount("/dev", 0, &stat);
  
  if (fd == -1) {
    exit(-1);
  }
  
  return 0;
}





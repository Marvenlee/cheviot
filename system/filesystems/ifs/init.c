/*
 * Copyright 2014  Marven Gilhespie
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
#include "ifs.h"
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


/* @brief   Initialize IFS
 *
 */
void init_ifs(int argc, char *argv[])
{
  log_info("ifs - init");
  
  if (argc < 3) {
    log_error("ifs, too few arguments, argc:%d", argc);
    exit (EXIT_FAILURE);
  }

  log_info("argv[1] = %s", argv[1]);
  log_info("argv[2] = $s", argv[2]);

  ifs_image = (void *)strtoul(argv[1], NULL, 16);
  ifs_image_size = strtoul(argv[2], NULL, 10);

  log_info("ifs image = %08x", (uint32_t)ifs_image);
  log_info("ifs image size = %u", (uint32_t)ifs_image_size);

  
  ifs_header = (struct IFSHeader *) ifs_image;
  
  if (ifs_header->magic[0] != 'M' || ifs_header->magic[1] != 'A' ||
      ifs_header->magic[2] != 'G' || ifs_header->magic[3] != 'C') {
    log_error("IFS magic header not found");
    exit(EXIT_FAILURE);
  }

  ifs_inode_table = (struct IFSNode *)((uint8_t *)ifs_header + ifs_header->node_table_offset);

  log_info("mounting root");

  mount_root();
  
  log_info("root mounted");
}


/* @brief   Create a mount point for root at "/".
 *
 */
void mount_root(void)
{
  struct stat stat;

  stat.st_dev = 0;              // Get from config, or returned by Mount() (sb index?)
  stat.st_ino = 0;
  stat.st_mode = 0777 | _IFDIR; // default to read/write of device-driver uid/gid.
  stat.st_uid = 0;              // default device driver uid
  stat.st_gid = 0;              // default gid
  stat.st_blksize = 512;
  stat.st_size = 0;
  stat.st_blocks = stat.st_size / stat.st_blksize;

  portid = mount("/", 0, &stat);
  
  if (portid < 0) {
    log_error("failed to mount ifs as root");
    exit(-1);
  }

  kq = kqueue();
  
  if (kq < 0) {
    log_error("failed to create kqueue");
    exit(-1);  
  }  
}


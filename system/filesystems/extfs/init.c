/*
 * Copyright 2023  Marven Gilhespie
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

#define LOG_LEVEL_INFO

#include <stdarg.h>
#include "ext2.h"
#include "globals.h"


/* @brief   Initialize and mount an Ext filesystem
 *
 */
void init(int argc, char *argv[])
{
//  struct stat block_stat;
  struct stat mnt_stat;
  int sc;
  struct superblock *sp;
  
  memset (&superblock, 0, sizeof superblock);
  
  determine_cpu_endianness();
  
  sc = process_args(argc, argv);
  if (sc != 0) {
    panic("ext2fs failed to process command line arguments");
  }

  block_fd = open(sb_device_path, O_RDWR);

  if (block_fd == -1) {  
    panic("ext2fs failed to open block device");
  }

  // FIXME: Save the partition size returned by block stat, compare to
  // FIXME: Save the st_dev of the block device, this FS will be mounted with same value  
  // if (fstat(block_fd, &block_stat) != 0) {
  //   log_warn("ext2fs fstart failed");
  // }
  
  if (read_superblock() != 0) {
    panic("ext2fs failed to read superblock");
  }
  
  if ((cache = init_block_cache(block_fd, NR_CACHE_BLOCKS, sb_block_size)) == NULL) {
    panic("ext2fs init block cache failed");
  }

  if (init_inode_cache() != 0) {
    panic("ext2fs init inode cache failed");
  }
    
  mnt_stat.st_dev = 0;            // FIXME: Stat the block device to get dev value
  mnt_stat.st_ino = EXT2_ROOT_INO;
  mnt_stat.st_mode = 0777 | S_IFDIR; // FIXME: Get the RWX permissions from command line
  mnt_stat.st_uid = 0;            // FIXME: Get these from command line
  mnt_stat.st_gid = 0;            // FIXME: Get these from command line
                                  // Unless we inherit from "mounted on" dir
  mnt_stat.st_size = 0xFFFFFF00;  // FIXME: Get the size from partition or superblock
  mnt_stat.st_blksize = 512;
  mnt_stat.st_blocks = superblock.s_blocks_count; // Size in 512 byte blocks
  
  // FIXME:  Pass stat for root dir and files, pass additional statvfs for FS mount.
  portid = mount(sb_mount_path, 0, &mnt_stat);

  if (portid == -1) {
    panic("ext2fs mounting failed");
  }

  kq = kqueue();
  
  if (kq == -1) {
    panic("ext2fs kqueue failed");
  }  
}


/* @brief   Handle command line arguments
 *
 * @param   argc, number of command line arguments
 * @param   argv, array of pointers to command line argument strings
 * @return  0 on success, -1 otherwise
 *
 * -u default user-id
 * -g default gid
 * -m default mod bits
 * mount path (default arg)
 * device path
 */
int process_args(int argc, char *argv[])
{
  int c;
  
  sb_uid = 1000;
  sb_gid = 1000;
  sb_mode = 0777;
  sb_rd_only = false;
  
  if (argc <= 1) {
    return -1;
  }
    
  while ((c = getopt(argc, argv, "u:g:m:r")) != -1) {
    switch (c) {
      case 'u':
        sb_uid = atoi(optarg);
        break;

      case 'g':
        sb_gid = atoi(optarg);
        break;

      case 'm':
        sb_mode = atoi(optarg);
        break;

      case 'r':
        sb_rd_only = true;
        break;
      
      default:
        break;
    }
  }

  if (optind + 1 >= argc) {
    return -1;
  }

  sb_mount_path = argv[optind];
  sb_device_path = argv[optind + 1];
  return 0;
}



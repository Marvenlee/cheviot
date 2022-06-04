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

#include "fat.h"
#include "globals.h"
#include "sys/debug.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fsreq.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/syscalls.h>
#include <unistd.h>


void init(int argc, char *argv[]) {
  struct stat stat;
  int sc;
  
  KLog ("FAT argc = %d, argv = %08x", (uint32_t)argv);

  memset (&config, 0, sizeof config);

  sc = processArgs(argc, argv);
  if (sc != 0) {
    KLog("processArgs FAILED, sc = %d", sc);
    exit(-1);
  }

  KLog ("Opening block device (%s)", config.device_path);
  
  memset(bootsector, 0, 512);

  block_fd = open(config.device_path, O_RDWR);

  if (block_fd == -1) {
    KLog("Failed to open %s", config.device_path);
    exit(-1);
  }

  KLog("opened, block_fd = %d", block_fd);


  block_cache = CreateCache (block_fd, 64, 512, 0,0,1,0,512);
  
  if (block_cache == NULL) {
    KLog ("Failed to create cache");
    exit(-1);
  }

  stat.st_dev = 0;
  stat.st_ino = 0;
  stat.st_mode = 0777 | _IFDIR;
  stat.st_uid = 0;
  stat.st_gid = 0;

  //  stat.st_size = ;      // size of root directory
  stat.st_blksize = 512;
  //  stat.st_blocks = ;

  if (config.fat_format) {
    FatFormat("TEST", 0 , 512);    
  }
  
  if (detectPartition() == -1) {
    KLog("detectPartition failed");
    exit(-1);
  }
  
  
  KLog("Mounting %s", config.mount_path);
  fd = Mount(config.mount_path, 0, &stat);

  if (fd < 0) {
    KLog("***** exiting fat, mounting (%s) failed", config.mount_path);
    exit(-1);
  }
}

/*
 * -u default user-id
 * -g default gid
 * -m default mod bits
 * -D KLog level ?
 * mount path (default arg)
 * device path
 */
int processArgs(int argc, char *argv[]) {
  int c;


  if (argc <= 1) {
    KLog ("processArgs failed, argc = %d", argc);
    exit(-1);
  }
  
  for (int t = 0; t<argc; t++) {
    KLog ("FAT Arg %d = (%s)", t, argv[t]);
  }
  

  while ((c = getopt(argc, argv, "u:g:m:f")) != -1) {
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

    case 'f':
      config.fat_format = true;
      break;
    default:
      break;
    }
  }

  if (optind + 1 >= argc) {
    KLog ("processArgs failed, optind = %d, argc = %d", optind, argc);
    exit(-1);
  }

  strncpy(config.mount_path, argv[optind], sizeof config.mount_path);

  strncpy(config.device_path, argv[optind + 1], sizeof config.device_path);

  KLog ("mount_path (optind)    = %s", config.mount_path);
  KLog ("device_path (optind+1) = %s", config.device_path);

  return 0;
}

/*
 *
 */

int detectPartition(void) {
  struct MBRPartitionEntry mbe[4];
  int t;

  KLog(">>>>>>>>>>>>>>>>> FAT - FatInit <<<<<<<<<<<<");
  blockRead(bootsector, 512, 0);
  memcpy(mbe, bootsector + 446, 16 * 4);

  for (t = 0; t < 4; t++) {
    KLog("partition = %d", t);

    if (mbe[t].partition_type == 0x00) {
      continue;
    }

    fsb.partition_start = mbe[t].lba;
    fsb.partition_size = mbe[t].nsectors;

    KLog("Partition %d, sec = %d, sz = %d", t, mbe[t].lba, mbe[t].nsectors);

    blockRead(file_buf, 512, fsb.partition_start);
    file_buf_sector = 0;

    memcpy(&fsb.bpb, file_buf, sizeof(struct FatBPB));
    memcpy(&fsb.bpb16, file_buf + BPB_EXT_OFFSET, sizeof(struct FatBPB_16Ext));
    memcpy(&fsb.bpb32, file_buf + BPB_EXT_OFFSET, sizeof(struct FatBPB_32Ext));

    KLog("bytes_per_sector = %d", fsb.bpb.bytes_per_sector);

    if (fsb.bpb.bytes_per_sector != 512) {
      continue;
    }

    KLog("sectors_per_cluster = %d", fsb.bpb.sectors_per_cluster);

    if (!(fsb.bpb.sectors_per_cluster >= 1 &&
          fsb.bpb.sectors_per_cluster <= 128 &&
          (fsb.bpb.sectors_per_cluster & (fsb.bpb.sectors_per_cluster - 1)) ==
              0))
      continue;

    if (fsb.bpb.reserved_sectors_cnt == 0) {
      continue;
    }

    if (fsb.bpb.fat_cnt == 0) {
      continue;
    }

    if (!(fsb.bpb.media_type == 0 || fsb.bpb.media_type == 1 ||
          fsb.bpb.media_type >= 0xf0)) {
      continue;
    }

    if (!((fsb.bpb.total_sectors_cnt16 != 0) ||
          (fsb.bpb.total_sectors_cnt16 == 0 &&
           fsb.bpb.total_sectors_cnt32 != 0))) {
      continue;
    }

    if (!((fsb.bpb.sectors_per_fat16 != 0) ||
          (fsb.bpb.sectors_per_fat16 == 0 &&
           fsb.bpb32.sectors_per_fat32 != 0))) {
      continue;
    }

    fsb.root_dir_sectors =
        ((fsb.bpb.root_entries_cnt * sizeof(struct FatDirEntry)) + (511)) / 512;

    if (fsb.bpb.sectors_per_fat16 != 0)
      fsb.sectors_per_fat = fsb.bpb.sectors_per_fat16;
    else
      fsb.sectors_per_fat = fsb.bpb32.sectors_per_fat32;

    if (fsb.bpb.total_sectors_cnt16 != 0)
      fsb.total_sectors_cnt = fsb.bpb.total_sectors_cnt16;
    else
      fsb.total_sectors_cnt = fsb.bpb.total_sectors_cnt32;

    fsb.first_data_sector = fsb.bpb.reserved_sectors_cnt +
                            (fsb.bpb.fat_cnt * fsb.sectors_per_fat) +
                            fsb.root_dir_sectors;

    fsb.data_sectors =
        fsb.total_sectors_cnt -
        (fsb.bpb.reserved_sectors_cnt +
         (fsb.bpb.fat_cnt * fsb.sectors_per_fat) + fsb.root_dir_sectors);

    fsb.cluster_cnt = fsb.data_sectors / fsb.bpb.sectors_per_cluster;

    if (fsb.cluster_cnt < 4085) {
      KLog("FAT12");
      fsb.fat_type = TYPE_FAT12;
      continue;
    } else if (fsb.cluster_cnt < 65525) {
      fsb.fat_type = TYPE_FAT16;
      KLog("FAT16");
    } else {
      fsb.fat_type = TYPE_FAT32;
      KLog("FAT32");
    }

    if (fsb.fat_type == TYPE_FAT32 && fsb.bpb32.fs_version != 0)
      continue;

    if ((fsb.fat_type == TYPE_FAT12 || fsb.fat_type == TYPE_FAT16) &&
        fsb.bpb.root_entries_cnt == 0)
      continue;

    InitRootNode();

    KLog("FAT PARTITION FOUND");
    return 0;
  }

  KLog("No FAT partition");
  return -1;
}

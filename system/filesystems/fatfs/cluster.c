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


int ReadFATEntry(uint32_t cluster, uint32_t *r_value) {
  int32 sector, sector_offset;
  uint32_t fat_offset, alignment;
  uint16_t word_value;
  uint32_t long_value;
  int f;
  uint32_t fat_sz;

  for (f = 0; f < fsb.bpb.fat_cnt; f++) {
    if (fsb.fat_type == TYPE_FAT12) {
      fat_sz = fsb.bpb.sectors_per_fat16;
      fat_offset = cluster + cluster / 2;
      alignment = cluster % 2;

      sector = fsb.bpb.reserved_sectors_cnt + (fat_offset / 512) + (f * fat_sz);
      sector_offset = fat_offset % 512;

      if (readSector(&word_value, sector, sector_offset, 2) != 0)
        break;

      if (alignment == 1)
        word_value = word_value >> 4;
      else
        word_value = word_value & 0x0fff;

      if (word_value >= FAT12_CLUSTER_EOC_MIN &&
          word_value <= FAT12_CLUSTER_EOC_MAX)
        *r_value = CLUSTER_EOC;
      else if (word_value == FAT12_CLUSTER_BAD)
        *r_value = CLUSTER_BAD;
      else
        *r_value = word_value;

      return 0;
    } else if (fsb.fat_type == TYPE_FAT16) {
      fat_sz = fsb.bpb.sectors_per_fat16;
      fat_offset = cluster * 2;

      sector = fsb.bpb.reserved_sectors_cnt + (fat_offset / 512) + (f * fat_sz);
      sector_offset = fat_offset % 512;

      if (readSector(&word_value, sector, sector_offset, 2) != 0)
        break;

      if (word_value >= FAT16_CLUSTER_EOC_MIN)
        *r_value = CLUSTER_EOC;
      else if (word_value == FAT16_CLUSTER_BAD)
        *r_value = CLUSTER_BAD;
      else
        *r_value = word_value;

      return 0;
    } else {
      fat_sz = fsb.bpb32.sectors_per_fat32;
      fat_offset = cluster * 4;

      sector = fsb.bpb.reserved_sectors_cnt + (fat_offset / 512) + (f * fat_sz);
      sector_offset = fat_offset % 512;

      if (readSector(&long_value, sector, sector_offset, 4) != 0)
        break;

      if (long_value >= FAT32_CLUSTER_EOC_MIN &&
          long_value <= FAT32_CLUSTER_EOC_MAX)
        *r_value = CLUSTER_EOC;
      else if (long_value == FAT32_CLUSTER_BAD)
        *r_value = CLUSTER_BAD;
      else
        *r_value = long_value;

      return 0;
    }
  }

  return -1;
}

/*
 * WriteFATEntry();
 */

int WriteFATEntry(uint32_t cluster, uint32_t value) {
  uint16_t word_value;
  uint32_t long_value;
  int32_t sector, sector_offset;
  uint32_t fat_offset, alignment;
  int f;
  int fat_written_cnt = 0;
  uint32_t fat_sz;

  for (f = 0; f < fsb.bpb.fat_cnt; f++) {
    if (fsb.fat_type == TYPE_FAT12) {
      /* FIXME: Do a check if cluster == 0 or cluster == 1, special case
       * clusters */

      if (value == CLUSTER_EOC)
        value = FAT12_CLUSTER_EOC;
      else if (value == CLUSTER_BAD)
        value = FAT12_CLUSTER_BAD;

      fat_sz = fsb.bpb.sectors_per_fat16;
      fat_offset = cluster + cluster / 2;
      alignment = cluster % 2;

      sector = fsb.bpb.reserved_sectors_cnt + (fat_offset / 512) + (f * fat_sz);
      sector_offset = fat_offset % 512;

      if (readSector(&word_value, sector, sector_offset, 2) != 0)
        break;

      if (alignment == 1)
        word_value = ((value << 4) & 0xfff0) | (word_value & 0x000f);
      else
        word_value = (value & 0x0fff) | (word_value & 0xf000);

      if (writeSector(&word_value, sector, sector_offset, 2) != 0)
        break;

      fat_written_cnt++;
    } else if (fsb.fat_type == TYPE_FAT16) {
      /* FIXME: Do a check if cluster == 0 or cluster == 1, special case
       * clusters */

      if (value == CLUSTER_EOC)
        value = FAT16_CLUSTER_EOC;
      else if (value == CLUSTER_BAD)
        value = FAT16_CLUSTER_BAD;

      fat_sz = fsb.bpb.sectors_per_fat16;
      fat_offset = cluster * 2;

      sector = fsb.bpb.reserved_sectors_cnt + (fat_offset / 512) + (f * fat_sz);
      sector_offset = fat_offset % 512;

      word_value = value;

      if (writeSector(&word_value, sector, sector_offset, 2) != 0)
        break;

      fat_written_cnt++;
    } else {
      /* FIXME: Do a check if cluster == 0 or cluster == 1, special case
       * clusters */

      if (value == CLUSTER_EOC)
        value = FAT32_CLUSTER_EOC;
      else if (value == CLUSTER_BAD)
        value = FAT32_CLUSTER_BAD;

      fat_sz = fsb.bpb32.sectors_per_fat32;
      fat_offset = cluster;

      sector = fsb.bpb.reserved_sectors_cnt + (fat_offset / 512) + (f * fat_sz);
      sector_offset = fat_offset % 512;

      long_value = value;

      if (writeSector(&long_value, sector, sector_offset, 4) != 0)
        break;

      fat_written_cnt++;
    }
  }

  /* FIXME:  really need primary FAT table to be written to consider it a
   * success */

  if (fat_written_cnt == 0)
    return -1;
  else
    return 0;
}

/*
 * AppendCluster();
 *
 * Clearing the directory cluster may fail, but we carry on regardless
 *
 */

int AppendCluster(struct FatNode *node, uint32_t *r_cluster) {

  uint32_t last_cluster;
  uint32_t cluster;

  if (FindLastCluster(node, &last_cluster) == 0) {
    if (FindFreeCluster(&cluster) == 0) {
      if (GetFirstCluster(&node->dirent) == CLUSTER_EOC) {
        SetFirstCluster(&node->dirent, cluster);
        FlushDirent(node);
      } else {
        WriteFATEntry(last_cluster, cluster);
      }

      *r_cluster = cluster;
      return 0;
    }
  }

  return -1;
}

/*
 * FindLastCluster();
 *
 * Or return -1 on error and return CLUSTER_EOC when there is no first cluster.
 * What about CLUSTER_BAD ?????
 */

int FindLastCluster(struct FatNode *node, uint32_t *r_cluster) {
  uint32_t cluster, next_cluster;

  if (GetFirstCluster(&node->dirent) == CLUSTER_EOC) {
    *r_cluster = CLUSTER_EOC;
    return 0;
  } else {
    if (node->hint_cluster != 0)
      cluster = node->hint_cluster;
    else
      cluster = GetFirstCluster(&node->dirent);

    if (ReadFATEntry(cluster, &next_cluster) != 0)
      return -1;

    while (next_cluster >= CLUSTER_ALLOC_MIN &&
           next_cluster <= CLUSTER_ALLOC_MAX) {
      cluster = next_cluster;

      node->hint_offset += fsb.bpb.sectors_per_cluster * 512;
      node->hint_cluster = cluster;

      if (ReadFATEntry(cluster, &next_cluster) != 0)
        return -1;
    }

    *r_cluster = cluster;

    return 0;
  }
}

int FindCluster(struct FatNode *node, off_t offset, uint32_t *r_cluster) {
  uint32_t cluster_size;
  uint32_t temp_offset;
  uint32_t cluster;
  uint32_t new_cluster;

  cluster_size = fsb.bpb.sectors_per_cluster * 512;
  offset = (offset / cluster_size) * cluster_size;

//  KLog ("fat: FindCluster offs %d", offset);

/*
  if (node->hint_cluster != 0 && offset == node->hint_offset) {
    *r_cluster = node->hint_cluster;
    return 0;
  } else if (node->hint_cluster != 0 && offset > node->hint_offset) {
    temp_offset = node->hint_offset;
    cluster = node->hint_cluster;
  } 
*/
  
  if (node->is_root == true) {
    temp_offset = 0;
    cluster = fsb.bpb32.root_cluster;
  } else {
//    KLog ("********* node is not root ***********");
    temp_offset = 0;
    cluster = GetFirstCluster(&node->dirent);
  }

  while (temp_offset < offset) {
    if (cluster < CLUSTER_ALLOC_MIN || cluster > CLUSTER_ALLOC_MAX) {
      KLog ("FindCLuster -EIO bounds");
      return -EIO;
    }

    if (ReadFATEntry(cluster, &new_cluster) != 0) {
      KLog ("FindCLuster -EIO ReadFatEntry");
      return -EIO;
    }
    
    cluster = new_cluster;
    temp_offset += cluster_size;
  }

  if (cluster < CLUSTER_ALLOC_MIN || cluster > CLUSTER_ALLOC_MAX) {
    KLog ("FindCLuster -EIO Bounds temp_offset >= offset");
    return -EIO;
  } else {
    *r_cluster = cluster;
    
//      KLog ("fat: FindCluster found %d", cluster);

    return 0;
  }
}


/*
 * GetFirstCluster();
 */

uint32_t GetFirstCluster(struct FatDirEntry *dirent) {
  uint32_t cluster;

  if (fsb.fat_type == TYPE_FAT12) {
    cluster = dirent->first_cluster_lo;

    if (cluster >= FAT12_CLUSTER_EOC_MIN && cluster <= FAT12_CLUSTER_EOC_MAX)
      return CLUSTER_EOC;
    else if (cluster == FAT12_CLUSTER_BAD)
      return CLUSTER_BAD;
    else
      return cluster;
  } else if (fsb.fat_type == TYPE_FAT16) {
    cluster = dirent->first_cluster_lo;

    if (cluster >= FAT16_CLUSTER_EOC_MIN && cluster <= FAT16_CLUSTER_EOC_MAX)
      return CLUSTER_EOC;
    else if (cluster == FAT16_CLUSTER_BAD)
      return CLUSTER_BAD;
    else
      return cluster;
  } else {
    cluster = (dirent->first_cluster_hi << 16) + dirent->first_cluster_lo;

    if (cluster >= FAT32_CLUSTER_EOC_MIN && cluster <= FAT32_CLUSTER_EOC_MAX)
      return CLUSTER_EOC;
    else if (cluster == FAT32_CLUSTER_BAD)
      return CLUSTER_BAD;
    else
      return cluster;
  }
}

/*
 * SetFirstCluster();
 *
 * ** ** Write to disk
 */

void SetFirstCluster(struct FatDirEntry *dirent, uint32_t cluster) {
  if (fsb.fat_type == TYPE_FAT12) {
    if (cluster == CLUSTER_EOC)
      cluster = FAT12_CLUSTER_EOC;
    else if (cluster == CLUSTER_BAD)
      cluster = FAT12_CLUSTER_BAD;

    dirent->first_cluster_hi = 0;
    dirent->first_cluster_lo = cluster & 0x00000fff;
  } else if (fsb.fat_type == TYPE_FAT16) {
    if (cluster == CLUSTER_EOC)
      cluster = FAT16_CLUSTER_EOC;
    else if (cluster == CLUSTER_BAD)
      cluster = FAT16_CLUSTER_BAD;

    dirent->first_cluster_hi = 0;
    dirent->first_cluster_lo = cluster & 0x0000ffff;
  } else {
    if (cluster == CLUSTER_EOC)
      cluster = FAT32_CLUSTER_EOC;
    else if (cluster == CLUSTER_BAD)
      cluster = FAT32_CLUSTER_BAD;

    dirent->first_cluster_hi = (cluster >> 16) & 0x0000ffff;
    dirent->first_cluster_lo = cluster & 0x0000ffff;
  }
}

/*
 * FindFreeCluster();
 */

int FindFreeCluster(uint32_t *r_cluster) {
  uint32_t cluster;
  uint32_t value;

  for (cluster = fsb.start_search_cluster; cluster < fsb.cluster_cnt;
       cluster++) {
    if (ReadFATEntry(cluster, &value) == 0) {
      if (value == CLUSTER_FREE) {
        if (WriteFATEntry(cluster, CLUSTER_EOC) == 0) {
          *r_cluster = cluster;
          fsb.start_search_cluster = cluster;
          /*
                                                  if (fsb.fsinfo_valid)
                                                  {
                                                          fsb.fsi.next_free =
             cluster;
                                                          fsb.fsi.free_cnt --;
                                                          FlushFSKLog();
                                                  }
          */
          return 0;
        }
      }
    }
  }

  for (cluster = 2; cluster < fsb.start_search_cluster; cluster++) {
    if (ReadFATEntry(cluster, &value) == 0) {
      if (value == CLUSTER_FREE) {
        if (WriteFATEntry(cluster, CLUSTER_EOC) == 0) {
          *r_cluster = cluster;
          fsb.start_search_cluster = cluster;
          /*
                                                  if (fsb.fsinfo_valid)
                                                  {
                                                          fsb.fsi.next_free =
             cluster;
                                                          fsb.fsi.free_cnt --;
                                                          FlushFSKLog();
                                                  }
          */
          return 0;
        }
      }
    }
  }

  return -1;
}

/*
 * FreeClusters();
 *
 * Modify so that it takes a node as a parameter,  only free upto official
 * filesize cluster.  But remember we may not be freeing clusters starting
 * from the beginning of a file.
 */

void FreeClusters(uint32_t first_cluster) {
  uint32_t cluster, next_cluster;

  cluster = first_cluster;

  while (cluster >= CLUSTER_ALLOC_MIN && cluster <= CLUSTER_ALLOC_MAX) {
    if (ReadFATEntry(cluster, &next_cluster) != 0)
      return;

    if (WriteFATEntry(cluster, CLUSTER_FREE) != 0)
      return;

    cluster = next_cluster;

    //		if (fsb.fsinfo_valid)
    //			fsb.fsi.free_cnt ++;
  }

  FlushFSInfo();
}

/*
 * ClusterToSector();
 */

uint32_t ClusterToSector(uint32_t cluster) {
  uint32_t sector;

  sector =
      ((cluster - 2) * fsb.bpb.sectors_per_cluster) + fsb.first_data_sector;

  return sector;
}

/*
 * FIX  Should return an error???????  Or is that only possible with buggy code
 *
 * Usually used during Lookup to find the sector/offset of a Dirent so it
 * assumes the file length-cluster is valid at that point.
 */

void FileOffsetToSectorOffset(struct FatNode *node, off_t file_offset,
                              uint32_t *r_sector, uint32_t *r_sec_offset) {
  uint32_t cluster;
  uint32_t base_sector;
  uint32_t cluster_offset;
  uint32_t sector_in_cluster;

  if (node == &fsb.root_node &&
      (fsb.fat_type == TYPE_FAT12 || fsb.fat_type == TYPE_FAT16)) {
    *r_sector = fsb.bpb.reserved_sectors_cnt +
                (fsb.bpb.fat_cnt * fsb.sectors_per_fat) + (file_offset / 512);

    *r_sec_offset = file_offset % 512;
  } else if (FindCluster(node, file_offset, &cluster) == 0) {
    base_sector = ClusterToSector(cluster);

    cluster_offset = file_offset % (fsb.bpb.sectors_per_cluster * 512);
    sector_in_cluster = cluster_offset / 512;

    *r_sector = base_sector + sector_in_cluster;
    *r_sec_offset = file_offset % 512;
  } else {
    KLog("File Offset to Sector Offset FAILURE, shouldn't happen");
    exit(-1);
  }
}

/*
 * ClearCluster();
 *
 * Attempts to clear a cluster, returns 0 on success.
 */

int ClearCluster(uint32_t cluster) {
  uint32_t sector;
  int c;
  uint8_t clear_sector[512];

  memset(clear_sector, 0, 512);

  sector = ClusterToSector(cluster);

  for (c = 0; c < fsb.bpb.sectors_per_cluster; c++) {
    if (writeSector(clear_sector, sector + c, 0, 512) != 0)
      return -1;
  }

  return 0;
}

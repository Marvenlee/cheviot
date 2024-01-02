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


/*
 *
 */
size_t readFile(struct FatNode *node, void *buf, size_t count, off_t offset) {
  uint32_t cluster;
  uint32_t nbytes_read;
  uint32_t transfer_nbytes;
  uint32_t sector;
  uint32_t cluster_offset;
  uint32_t sector_offset;
  uint8_t *dst;


  if (offset >= node->dirent.size)
    count = 0;
  else if (count < (node->dirent.size - offset))
    count = count;
  else
    count = node->dirent.size - offset;

  dst = buf;
  nbytes_read = 0;

  while (nbytes_read < count) {
    /*
     * Fix, assuming Cluster is 512 bytes.
     * Should also read nclusters to see if they're contiguous.
     */

    if (FindCluster(node, offset, &cluster) != 0)
      break;

    cluster_offset = offset % (fsb.bpb.sectors_per_cluster * 512);
    sector = ClusterToSector(cluster) + (cluster_offset / 512);

    sector_offset = offset % 512;

    transfer_nbytes =
        (512 < (count - nbytes_read)) ? 512 : (count - nbytes_read);
    transfer_nbytes = (transfer_nbytes < (512 - sector_offset))
                          ? transfer_nbytes
                          : (512 - sector_offset);

//    KLog ("fat: readFile dst = %08x, file_offset = %d, xfer = %d", (uint32_t)dst, offset, transfer_nbytes);
    
    if (readSector(dst, sector, sector_offset, transfer_nbytes) != 0) {
      break;
    }
    
    dst += transfer_nbytes;
    nbytes_read += transfer_nbytes;
    offset += transfer_nbytes;
  }

  //	FatSetTime (&node->dirent, ST_ATIME);

  return nbytes_read;
}

/*
 *
 */
size_t writeFile(struct FatNode *node, void *buf, size_t count, off_t offset) {
  uint32 cluster;
  uint32 sector;
  uint32 cluster_offset;
  uint32 transfer_nbytes;
  uint32 sector_offset;
  uint8 *src;
  uint32 nbytes_written;
  uint32 nbytes_to_clear;

  /* Clear from end of file to end of last cluster */

  if (offset > node->dirent.size) {
    nbytes_to_clear =
        (fsb.bpb.sectors_per_cluster * 512) -
        (node->dirent.size % (fsb.bpb.sectors_per_cluster * 512));

    if (FindCluster(node, node->dirent.size, &cluster) == 0) {
      cluster_offset = offset % (fsb.bpb.sectors_per_cluster * 512);
      sector = ClusterToSector(cluster) + (cluster_offset / 512);
      sector_offset = offset % 512;

      if (writeSector(NULL, sector, sector_offset, nbytes_to_clear) != 0)
        return -1;
    }
  }

  src = buf;
  nbytes_written = 0;

  while (nbytes_written < count) {
    while (FindCluster(node, offset, &cluster) != 0) {
      if (AppendCluster(node, &cluster) != 0)
        return nbytes_written;
    }

    cluster_offset = offset % (fsb.bpb.sectors_per_cluster * 512);
    sector = ClusterToSector(cluster) + (cluster_offset / 512);
    sector_offset = offset % 512;

    transfer_nbytes =
        (512 < (count - nbytes_written)) ? 512 : (count - nbytes_written);
    transfer_nbytes = (transfer_nbytes < (512 - sector_offset))
                          ? transfer_nbytes
                          : (512 - sector_offset);

    if (writeSector(src, sector, sector_offset, transfer_nbytes) != 0)
      return nbytes_written;

    src += transfer_nbytes;
    offset += transfer_nbytes;
    nbytes_written += transfer_nbytes;
  }

  if (offset > node->dirent.size)
    node->dirent.size = offset;

  //	FatSetTime (&node->dirent, ST_MTIME | ST_CTIME);

  /* FIXME:  Really need a FatFlushNode() */

  return nbytes_written;
}

/*
 *
 * If write fails, clear cached dirent
 */
struct FatNode *createFile(struct FatNode *parent, char *name) {
  uint32 sector;
  uint32 sector_offset;
  struct FatDirEntry dirent;
  struct FatNode *node;

  KLog("FatCreateFile()");

  if (FatASCIIZToDirEntry(&dirent, name) != 0) {
    goto exit;
  }

  dirent.attributes = 0;
  dirent.reserved = 0;
  dirent.first_cluster_hi = 0;
  dirent.first_cluster_lo = 0;
  dirent.size = 0;
  //	FatSetTime (&dirent, ST_CTIME | ST_ATIME | ST_MTIME);
  SetFirstCluster(&dirent, CLUSTER_EOC);

  if (FatCreateDirEntry(parent, &dirent, &sector, &sector_offset) == 1) {
    if ((node = AllocNode(&dirent, sector, sector_offset)) != NULL) {
      //			FatSetTime (&parent->dirent, ST_CTIME | ST_ATIME
      //| ST_MTIME);
      FlushDirent(parent);
      return node;
    }

    FatDeleteDirEntry(sector, sector_offset);
  }

exit:
  return NULL;
}

/*
 *
 */
int truncateFile(struct FatNode *node, size_t size) {
  uint32_t cluster;

  if (size == 0) {
    cluster = GetFirstCluster(&node->dirent);

    if (cluster != CLUSTER_EOC || cluster != CLUSTER_BAD)
      ;
    {
      FreeClusters(cluster);

      SetFirstCluster(&node->dirent, CLUSTER_EOC);

      node->dirent.size = 0;
      //			FatSetTime (&node->dirent, ST_MTIME);

      node->hint_cluster = 0;
      node->hint_offset = 0;

      return FlushDirent(node);
    }
  } else {
    if (FindCluster(node, size, &cluster) == 0) {
      if ((size % (fsb.bpb.sectors_per_cluster * 512)) == 0) {
        if (FindCluster(node, size - 1, &cluster) == 0) {
          FreeClusters(cluster);
          node->dirent.size = size;
          //					FatSetTime (&node->dirent,
          //ST_MTIME);

          node->hint_cluster = 0;
          node->hint_offset = 0;

          return FlushDirent(node);
        }
      } else {
        FreeClusters(cluster);
        node->dirent.size = size;

        node->hint_cluster = 0;
        node->hint_offset = 0;

        //				FatSetTime (&node->dirent, ST_MTIME);
        return FlushDirent(node);
      }
    }
  }

  return 0;
}

/*
 *
 */
int extendFile(struct FatNode *node, size_t length) {
  return writeFile(node, NULL, 0, length);
}

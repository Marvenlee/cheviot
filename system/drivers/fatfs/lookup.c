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


int lookup (struct FatNode *dirnode, char *name, struct FatNode **r_node) {
  struct FatDirEntry dirent;
  int32_t offset;
  uint32_t dirent_sector, dirent_offset;
  int32_t rc;
  struct FatNode *node;
  char tmpname[32];
  uint32_t cluster;
  
//  KLog ("fat: lookup: name = %s dirent = %08x, dirnode->inode_nr = %d", (uint32)&dirent, name, dirnode->inode_nr);

  offset = 0;

  do {
    // Why can't dirnode not cache a cluster instead of returning dirent_sector/dirent_offset?
  
    rc = fat_dir_read(dirnode, &dirent, offset, &dirent_sector, &dirent_offset);

    if (rc == 0) {
        FatDirEntryToASCIIZ(tmpname, &dirent);

      if (dirent.name[0] == DIRENTRY_FREE) {
        offset++;
        continue;
      }

      if (dirent.name[0] != DIRENTRY_DELETED) {
        if (fat_cmp_dirent(&dirent, name) == 0) {

          cluster = GetFirstCluster(&dirent);
          
          if (cluster == CLUSTER_EOC || cluster == CLUSTER_BAD) {
            KLog ("fat: lookup %s failed, cluster is zero length", name);
            return -EIO;
          }

          node = FindNode(cluster);
          
          if (node == NULL) {
            KLog ("fat: node does not already exist, allocating...");
            node = AllocNode(&dirent, dirent_sector, dirent_offset);
          }
                    
          if (node == NULL) {
            *r_node = NULL;
            KLog ("Failed to AllocNode, sector %d, offset %d", dirent_sector, dirent_offset);
            return -1;
          }

          node->inode_nr =
              (dirent.first_cluster_hi << 16) | dirent.first_cluster_lo;
          node->hint_cluster = 0;
          node->hint_offset = 0;

//          KLog ("fat: memcpy lookup dirent");
          memcpy(&node->dirent, &dirent, sizeof(dirent));
          
          *r_node = node;
          return 0;
        }
      }

      offset++;
    }

  } while (rc == 0);

  KLog ("Fat : lookup failed, rc = %d", rc);

  *r_node = NULL;
  return -1;
}




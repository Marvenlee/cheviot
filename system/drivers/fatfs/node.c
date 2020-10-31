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
 * ObtainNode();
 *
 * HAven't a clue, assume searches incore list for a given node based on
 * cluster and offset.  Then copies dirent to the node if the node is new.
 * Strange.
 */

struct FatNode *FindNode(ino_t inode_nr) {
  struct FatNode *node;

  node = LIST_HEAD(&fsb.node_list);

  while (node != NULL) {
    if (node->inode_nr == inode_nr) {
      node->reference_cnt++;
      break;
    }

    node = LIST_NEXT(node, node_entry);
  }
  
  if (node == NULL) {
    KLog ("fat: findnode failed");
  }

  return node;
}

/*
 *
 */

void InitRootNode() {
  struct FatNode *node;

  node = &fsb.root_node;

  memset(&node->dirent, 0, sizeof(struct FatDirEntry));

  node->dirent.attributes = ATTR_DIRECTORY;
  node->fsb = &fsb;
  node->inode_nr = 0;
  node->reference_cnt = 1;
  node->dirent_sector = 0;
  node->dirent_offset = 0;
  node->hint_cluster = 0;
  node->hint_offset = 0;
  node->is_root = true;

  LIST_ADD_HEAD(&fsb.node_list, node, node_entry);
}

/*
 * AllocNode();
 */

struct FatNode *AllocNode(struct FatDirEntry *dirent, uint32_t sector,
                          uint32_t offset) {
  struct FatNode *node;
  uint32_t cluster;
  
//  KLog ("fat: AllocNode");

  node = malloc(sizeof *node);
  
  if (node == NULL) {
    KLog ("fat: AllocNode failed");
    return NULL;
  }

//  KLog ("fat: allocnode memcpy after malloc %08x", (uint32_t)node);
  memcpy(&node->dirent, dirent, sizeof(struct FatDirEntry));

  cluster = GetFirstCluster(dirent);
  
  if (cluster == CLUSTER_EOC || cluster == CLUSTER_BAD) {
    KLog ("fat: zero-length file, unable to assign ino_nr");
    free(node);
    return NULL;
  }
  
  node->inode_nr = cluster;  
  node->fsb = &fsb;
  node->reference_cnt = 1;
  node->dirent_sector = sector;
  node->dirent_offset = offset;

  node->hint_cluster = 0;
  node->hint_offset = 0;

  LIST_ADD_TAIL(&fsb.node_list, node, node_entry);

  return node;
}

/*
 * FreeNode();
 */

void FreeNode(struct FatNode *node) {
  node->reference_cnt--;

  if (node == &fsb.root_node) {
    return;
  }

  //	FlushDirent (fsb, node);

  if (node->reference_cnt == 0) {
    LIST_REM_ENTRY(&fsb.node_list, node, node_entry);
    free(node);
  }
}

/*
 * FlushNode();
 */

/*
int FlushDirent (struct FatSB *fsb, struct FatNode *node)
{
        fsb = node->fsb;

        if (fsb->validated == FALSE || node == &fsb->root_node ||
fsb->write_protect == TRUE)
                return 0;

        WriteBlocks (fsb, &node->dirent, node->dirent_sector,
node->dirent_offset, sizeof (struct FatDirEntry), BUF_IMMED);

        return 0;
}
*/

/*
 * Not needed anywhere
 */

int FlushFSB(void) { return 0; }

/*
 * Optimize write of FSInfo
 */

void FlushFSInfo(void) {}

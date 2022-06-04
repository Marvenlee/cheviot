/*
 * Copyright 2014  Marven Gilhespie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
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

#include <kernel/arm/boot.h>
#include <kernel/arm/globals.h>
#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>

static void InitFSLists(void);
static void InitCache(void);

/*
 *
 */
int InitVFS(void) {
  Info("InitVFS()");

  InitFSLists();
  InitCache();
  InitPipes();
  
  root_vnode = NULL;
     
  Info("InitVFS done");
  return 0;
}

static void InitFSLists(void) {

  LIST_INIT(&vnode_free_list);
  LIST_INIT(&filp_free_list);
  LIST_INIT(&dname_lru_list);
  LIST_INIT(&poll_free_list);
  LIST_INIT(&free_superblock_list);
  
  // TODO: Need pagetables allocated for this file cache.

  // TODO Replace NR_VNODE, NR_FILP, NR_DNAME with computed variables max_vnode,
  // max_filp etc.
  // Allocate in main.c

  // Perhaps get some params from kernel command line?

  for (int t = 0; t < NR_VNODE; t++) {
    LIST_ADD_TAIL(&vnode_free_list, &vnode_table[t], vnode_entry);
  }

  for (int t = 0; t < NR_FILP; t++) {
    LIST_ADD_TAIL(&filp_free_list, &filp_table[t], filp_entry);
  }

  for (int t = 0; t < NR_DNAME; t++) {
    LIST_ADD_TAIL(&dname_lru_list, &dname_table[t], lru_link);
    dname_table[t].hash_key = -1;
  }

  for (int t = 0; t < DNAME_HASH; t++) {
    LIST_INIT(&dname_hash[t]);
  }

  for (int t = 0; t < NR_POLL; t++) {
    LIST_ADD_TAIL(&poll_free_list, &poll_table[t], poll_link);
  }

  for (int t = 0; t < max_superblock; t++) {
    LIST_ADD_TAIL(&free_superblock_list, &superblock_table[t], link);
  }
}

/**
 *
 */
static void InitCache(void) {
  vm_addr va;

  LIST_INIT(&buf_avail_list);

  va = CACHE_BASE_VA;

  for (int t = 0; t < max_buf; t++) {
    InitRendez(&buf_table[t].rendez);
    buf_table[t].flags = 0;
    buf_table[t].vnode = NULL;
    buf_table[t].blockdev = NULL;
    buf_table[t].cluster_offset = 0;
    buf_table[t].cluster_size = 0;
    buf_table[t].addr = (void *)va;
    // TODO: Remove    buf_table[t].offset = 0;
    //	    buf_table[t].remaining = 0;
    buf_table[t].dirty_bitmap = 0;

    va += CLUSTER_SZ;

    LIST_ADD_TAIL(&buf_avail_list, &buf_table[t], free_link);
  }

  for (int t = 0; t < BUF_HASH; t++) {
    LIST_INIT(&buf_hash[t]);
  }
}



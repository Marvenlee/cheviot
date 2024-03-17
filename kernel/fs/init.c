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

#include <kernel/board/boot.h>
#include <kernel/board/globals.h>
#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/msg.h>

static void InitFSLists(void);
static void InitCache(void);


/* @brief   Initialize the kernel's virtual filesystem
 */
int init_vfs(void)
{  
  InitFSLists();
  InitCache();
  InitPipes();
  
  root_vnode = NULL;  
  return 0;
}


/* @brief   Initialize tables and lists of VFS objects
 */
static void InitFSLists(void)
{
  LIST_INIT(&vnode_free_list);
  LIST_INIT(&filp_free_list);
  LIST_INIT(&dname_lru_list);
  LIST_INIT(&free_superblock_list);
  LIST_INIT(&kqueue_free_list);
  LIST_INIT(&knote_free_list);
  LIST_INIT(&isr_handler_free_list);

  // TODO: Need pagetables allocated for this file cache?
  // TODO Replace NR_VNODE, NR_FILP, NR_DNAME with computed variables max_vnode,
  // max_filp etc, that are allocated in main.c
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

  for (int t = 0; t < max_superblock; t++) {
    LIST_ADD_TAIL(&free_superblock_list, &superblock_table[t], link);
  }

  for (int t = 0; t < max_kqueue; t++) {
    LIST_ADD_TAIL(&kqueue_free_list, &kqueue_table[t], free_link);
  }

  for (int t = 0; t < max_knote; t++) {
    LIST_ADD_TAIL(&knote_free_list, &knote_table[t], link);
  }

  for (int t = 0; t < max_isr_handler; t++) {
    LIST_ADD_TAIL(&isr_handler_free_list, &isr_handler_table[t], free_link);
  }

  for (int t = 0; t < KNOTE_HASH_SZ; t++) {
    LIST_INIT(&knote_hash[t]);
  }  
}


/* @brief   Initialize the VFS's file cache
 *
 */
static void InitCache(void)
{
  vm_addr va;

  LIST_INIT(&buf_avail_list);

  va = CACHE_BASE_VA;

  for (int t = 0; t < max_buf; t++) {
    InitRendez(&buf_table[t].rendez);
    buf_table[t].flags = 0;
    buf_table[t].vnode = NULL;
    buf_table[t].cluster_offset = 0;
    buf_table[t].data = (void *)va;

    va += CLUSTER_SZ;

    LIST_ADD_TAIL(&buf_avail_list, &buf_table[t], free_link);
  }

  for (int t = 0; t < BUF_HASH; t++) {
    LIST_INIT(&buf_hash[t]);
  }
}


/*
 *
 */
void InitPipes(void)
{
  struct Pipe *pipe;
  
  LIST_INIT(&free_pipe_list);
  
  for (int t=0; t<max_pipe; t++) {
    LIST_ADD_TAIL(&free_pipe_list, &pipe_table[t], link);
  }
}


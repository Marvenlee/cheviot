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

#include <kernel/types.h>
#include <kernel/filesystem.h>
#include <kernel/proc.h>
#include <kernel/kqueue.h>
#include <kernel/interrupt.h>
#include <kernel/msg.h>

/*
 * Filesystem
 */
struct VNode *root_vnode;

int max_superblock;
struct SuperBlock *superblock_table;
superblock_list_t free_superblock_list;

int max_vnode;
struct VNode *vnode_table;
vnode_list_t vnode_free_list;

int max_filp;
struct Filp *filp_table;
filp_list_t filp_free_list;

int max_pipe;
struct Pipe *pipe_table;
pipe_list_t free_pipe_list;
struct SuperBlock pipe_sb;


/*
 * VFS file cache
 */ 
struct Process *bdflush_process; 
 
int max_buf;
struct Buf *buf_table;

size_t cluster_size;
int nclusters;    // FIXME: Different to max_buf ?
struct Rendez buf_list_rendez;

buf_list_t buf_hash[BUF_HASH];
buf_list_t buf_avail_list;

/*
 * Directory Name Lookup Cache
 */
struct DName dname_table[NR_DNAME];
dname_list_t dname_lru_list;
dname_list_t dname_hash[DNAME_HASH];

/*
 * VFS kqueue and knote event handling
 */
int max_kqueue;
struct KQueue *kqueue_table;
kqueue_list_t kqueue_free_list;

int max_knote;
struct KNote *knote_table;
knote_list_t knote_free_list;
knote_list_t knote_hash[KNOTE_HASH_SZ];

/*
 * TODO: VNode for sending system logs to a user-mode /procfs driver
 */
struct VNode *logger_vnode = NULL;

/*
 * MsgID table management
 */
 
msgid_t unique_msgid_counter = 0; 
msg_list_t msgid_hash[MSGID_HASH_SZ];





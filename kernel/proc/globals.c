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

#include <kernel/filesystem.h>
#include <kernel/proc.h>
#include <kernel/types.h>

/*
 * Memory
 */
 
 
struct Rendez root_rendez;
struct Process *ifs_process;

struct Process *timer_process;

 

vm_size mem_size;
int max_pageframe;
struct Pageframe *pageframe_table;

pageframe_list_t free_4k_pf_list;
pageframe_list_t free_16k_pf_list;
pageframe_list_t free_64k_pf_list;

/*
 * Timer
 */
timer_list_t timing_wheel[JIFFIES_PER_SECOND];
struct Rendez timer_rendez;

// timer_list_t free_timer_list;
// int max_timer;
// struct Timer *timer_table;

volatile spinlock_t timer_slock;

volatile long hardclock_time;
volatile long softclock_time;



superblock_list_t free_superblock_list;

/*
 *
 */
int max_cpu;
int cpu_cnt;
struct CPU cpu_table[MAX_CPU];

int max_process;
struct Process *process_table;

struct Process *root_process;
struct Process *bdflush_process;

/*
 * Scheduler
 */

process_circleq_t realtime_queue[32];
uint32_t realtime_queue_bitmap;
process_list_t stride_queue;
int stride_queue_cnt;
int global_tickets;
int global_stride;
int64_t global_pass;

int bkl_locked;
spinlock_t inkernel_now;
int inkernel_lock;

struct Process *bkl_owner;
process_list_t bkl_blocked_list;

/*
 * free counters (unused?)
 */
int free_pid_cnt;
int free_handle_cnt;
int free_process_cnt;
int free_timer_cnt;

/*
 * Sockets
 */

/*
 * Filesystem
 */

int max_superblock;
struct SuperBlock *superblock_table;

filp_list_t filp_free_list;
vnode_list_t vnode_free_list;
queue_list_t queue_free_list;


struct VNode *root_vnode;

int max_vnode;
struct VNode *vnode_table;

int max_filp;
struct Filp *filp_table;

int max_queue;
struct Queue *queue_table;

int max_pipe;
struct Pipe *pipe_table;
void *pipe_mem_base;
pipe_list_t free_pipe_list;

/*
 * Directory Name Lookup Cache
 */
struct DName dname_table[NR_DNAME];
dname_list_t dname_lru_list;
dname_list_t dname_hash[DNAME_HASH];

struct VNode *logger_vnode = NULL;

int max_poll;
struct Poll *poll_table;
poll_list_t poll_free_list;



/*
 * File Cache
 */
size_t cluster_size;
int nclusters;
struct Rendez buf_list_rendez;

int max_buf;

struct Buf *buf_table;

buf_list_t buf_hash[BUF_HASH];
buf_list_t buf_avail_list;

buf_list_t delayed_write_queue;

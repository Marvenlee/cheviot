#ifndef KERNEL_GLOBALS_H
#define KERNEL_GLOBALS_H

#include <kernel/filesystem.h>
#include <kernel/lists.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/vm.h>

/*
 * Memory
 */
 
extern struct Process *root_process;
extern struct Process *timer_process;

extern vm_size mem_size;
extern int max_pageframe;
extern struct Pageframe *pageframe_table;

extern pageframe_list_t free_4k_pf_list;
extern pageframe_list_t free_16k_pf_list;
extern pageframe_list_t free_64k_pf_list;

/*
 * Timer
 */
extern timer_list_t timing_wheel[JIFFIES_PER_SECOND];
extern struct Rendez timer_rendez;

// extern timer_list_t free_timer_list;
// extern int max_timer;
// extern struct Timer *timer_table;

extern volatile spinlock_t timer_slock;
extern volatile long hardclock_time;
extern volatile long softclock_time;
extern superblock_list_t free_superblock_list;

/*
 *
 */
extern int max_cpu;
extern int cpu_cnt;
extern struct CPU cpu_table[1];

extern int max_process;
extern struct Process *process_table;

extern struct Process *root_process;
extern struct Process *bdflush_process;

/*
 * Scheduler
 */

extern process_circleq_t realtime_queue[32];
extern uint32_t realtime_queue_bitmap;
extern process_list_t stride_queue;
extern int stride_queue_cnt;
extern int global_tickets;
extern int global_stride;
extern int64_t global_pass;

extern int bkl_locked;
extern spinlock_t inkernel_now;
extern int inkernel_lock;

extern struct Process *bkl_owner;
extern process_list_t bkl_blocked_list;

/*
 * free counters (unused?)
 */
extern int free_pid_cnt;
extern int free_handle_cnt;
extern int free_process_cnt;
extern int free_channel_cnt;
extern int free_timer_cnt;

/*
 * Sockets
 */
extern int max_socket;
extern struct Socket *socket_table;

/*
 * Filesystem
 */

extern int max_superblock;
extern struct SuperBlock *superblock_table;

extern filp_list_t filp_free_list;
extern vnode_list_t vnode_free_list;
extern queue_list_t queue_free_list;

extern struct VNode *root_vnode;

extern int max_vnode;
extern struct VNode *vnode_table;

extern int max_filp;
extern struct Filp *filp_table;

extern int max_pipe;
extern struct Pipe *pipe_table;
extern void *pipe_mem_base;
extern pipe_list_t free_pipe_list;

extern int max_poll;
struct Poll *poll_table;
poll_list_t poll_free_list;


/*
 * Directory Name Lookup Cache
 */
extern struct DName dname_table[NR_DNAME];
extern dname_list_t dname_lru_list;
extern dname_list_t dname_hash[DNAME_HASH];

extern struct VNode *logger_vnode;

/*
 * File Cache
 */
extern size_t cluster_size;
extern int nclusters;
extern struct Rendez buf_list_rendez;

extern int max_buf;

extern struct Buf *buf_table;

extern buf_list_t buf_hash[BUF_HASH];
extern buf_list_t buf_avail_list;

extern buf_list_t delayed_write_queue;

#endif

#ifndef KERNEL_GLOBALS_H
#define KERNEL_GLOBALS_H

#include <sys/syscalls.h>
#include <kernel/types.h>
#include <kernel/filesystem.h>
#include <kernel/lists.h>
#include <kernel/proc.h>
#include <kernel/vm.h>
#include <kernel/kqueue.h>
#include <kernel/interrupt.h>
#include <kernel/msg.h>



extern struct Process *root_process;
extern struct Process *timer_process;
extern struct Process *interrupt_dpc_process;

/*
 * Memory
 */
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
 * Interrupt
 */
extern int nirq;
extern struct Rendez interrupt_dpc_rendez;
extern isr_handler_list_t pending_isr_dpc_list;

extern int irq_mask_cnt[NIRQ];
extern int irq_handler_cnt[NIRQ];
extern isr_handler_list_t isr_handler_list[NIRQ];


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

extern process_circleq_t sched_queue[32];
extern uint32_t sched_queue_bitmap;

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

extern struct VNode *root_vnode;

extern int max_vnode;
extern struct VNode *vnode_table;

extern int max_filp;
extern struct Filp *filp_table;

extern int max_pipe;
extern struct Pipe *pipe_table;
extern pipe_list_t free_pipe_list;
extern struct SuperBlock pipe_sb;

extern int max_isr_handler;
extern struct ISRHandler *isr_handler_table;
extern isr_handler_list_t isr_handler_free_list;

extern int max_kqueue;
extern struct KQueue *kqueue_table;
extern kqueue_list_t kqueue_free_list;

extern int max_knote;
extern struct KNote *knote_table;
extern knote_list_t knote_free_list;

extern knote_list_t knote_hash[KNOTE_HASH_SZ];

// Messages
extern msgid_t unique_msgid_counter;
extern msg_list_t msgid_hash[MSGID_HASH_SZ];

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

superblock_list_t writable_filesystem_list;    





#endif

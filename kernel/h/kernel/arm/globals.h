#ifndef KERNEL_ARM_GLOBALS_H
#define KERNEL_ARM_GLOBALS_H

#include <kernel/types.h>
#include <kernel/lists.h>
#include <kernel/proc.h>
#include <kernel/vm.h>
#include <kernel/arm/arm.h>
#include <kernel/arm/raspberry.h>
#include <kernel/arm/task.h>






/*
 * 
 */

extern spinlock_t inkernel_now;
extern int inkernel_lock;



extern int free_handle_cnt;
extern int free_process_cnt;
extern int free_channel_cnt;
extern int free_timer_cnt;
extern int free_isr_handler_cnt;
// extern int free_notification_cnt;



/*
 * Linker script symbols
 */

/* 
extern uint8 _text_start;
extern uint8 _text_end;
extern uint8 _data_start;
extern uint8 _data_end;
extern uint8 _bss_start;
extern uint8 _bss_end;
*/

extern uint8 _ebss;

extern uint8 *kernel_heap_base;
extern uint32 kernel_heap_size;




/*
 *
 */
 
extern uint32 mailbuffer[64];
extern uint32 arm_mem_base;
extern uint32 arm_mem_size;

extern uint32 videocore_base;
extern uint32 videocore_ceiling;
extern uint32 videocore_phys;
 
extern volatile struct bcm2835_system_timer_registers *timer_regs;
extern volatile struct bcm2835_gpio_registers *gpio_regs;
extern volatile struct bcm2835_interrupt_registers *interrupt_regs;

extern bits32_t cpsr_dnm_state;



/*
 *
 */


extern void *idle_process_table;
extern void *process_table;
extern int max_process;

extern struct CPU cpu_table[MAX_CPU];
extern int max_cpu;
extern int cpu_cnt;

extern bool reschedule_request;

extern process_circleq_t realtime_queue[32];
extern uint32 realtime_queue_bitmap;
extern process_list_t stride_queue;
extern int stride_queue_cnt;
extern int global_tickets;
extern int global_stride;
extern int64 global_pass;

extern process_list_t free_process_list;

extern struct Process *root_process;
extern struct Process *idle_task;



/*
 * Debugger
 */

extern uint32 screen_width;
extern uint32 screen_height;
extern uint32 screen_buf;           // FIXME: void * 
extern uint32 screen_pitch;

extern vm_addr debug_base;
extern vm_addr debug_ceiling;
extern bool __debug_enabled;


/*
 *
 */
 
extern int max_isr_handler;
extern struct ISRHandler *isr_handler_table;
extern isrhandler_list_t isr_handler_list[NIRQ];
extern isrhandler_list_t free_isr_handler_list;
int irq_mask_cnt[NIRQ];
int irq_handler_cnt[NIRQ];
extern bits32_t mask_interrupts[3];
extern bits32_t pending_interrupts[3];


/*
 *
 */

extern struct BootInfo *bootinfo;
extern char *cfg_boot_prefix;
extern int cfg_boot_verbose;



/*
 *
 */
 
extern int max_handle;
extern struct Handle *handle_table;
extern handle_list_t free_handle_list;

extern int max_pagecache;
extern pagecache_list_t free_pagecache_list;
extern pagecache_list_t lru_pagecache_list;
extern pagecache_list_t pagecache_hash[NPAGECACHE_HASH];
extern struct Pagecache *pagecache_table;


extern vm_size mem_size;
extern int max_pageframe;
extern struct Pageframe *pageframe_table;
extern pageframe_list_t free_4k_pf_list;
extern pageframe_list_t free_16k_pf_list;
extern pageframe_list_t free_64k_pf_list;

extern uint32 free_pageframe_cnt;

extern vm_size overcommit_size;
extern vm_size cache_size;
extern vm_size dirty_cache_size;
extern vm_size min_cache_size;
extern vm_size max_cache_size;


/*
 *
 */

extern volatile long long hardclock_seconds;
extern volatile long hardclock_jiffies;
extern volatile long long softclock_seconds;
extern volatile long softclock_jiffies;

extern timer_list_t timing_wheel[JIFFIES_PER_SECOND];
extern timer_list_t free_timer_list;
extern int max_timer;
extern struct Timer *timer_table;

extern spinlock_t timer_slock;







#endif

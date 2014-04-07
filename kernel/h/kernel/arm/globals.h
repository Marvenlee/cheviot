#ifndef KERNEL_ARM_GLOBALS_H
#define KERNEL_ARM_GLOBALS_H

#include <kernel/types.h>
#include <kernel/lists.h>
#include <kernel/proc.h>
#include <kernel/vm.h>
#include <kernel/arm/arm.h>
#include <kernel/arm/raspberry.h>
#include <kernel/arm/task.h>


extern volatile struct bcm2835_system_timer_registers *timer_regs;
extern volatile struct bcm2835_gpio_registers *gpio_regs;
extern volatile struct bcm2835_interrupt_registers *interrupt_regs;

extern uint8 _text_start;
extern uint8 _text_end;
extern uint8 _data_start;
extern uint8 _data_end;
extern uint8 _bss_start;
extern uint8 _bss_end;


extern struct CPU cpu_table[MAX_CPU];

extern uint8 kernel_stack;
extern uint8 kernel_stack_top;

extern uint8 idle_stack;
extern uint8 idle_stack_top;


extern bits32_t cpsr_dnm_state;



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
extern int free_notification_cnt;



/*
 *
 */

extern struct BootInfo *bootinfo;
extern char *cfg_boot_prefix;
extern int cfg_boot_verbose;
extern uint32 estimated_heap_size;
extern uint32 text_base;
extern uint32 text_ceiling;
extern uint32 data_base;
extern uint32 data_ceiling;
extern uint32 heap_base;
extern uint32 heap_ceiling;
extern uint32 heap_ptr;



/*
 *
 */
 
extern uint32 mailbuffer[64];
extern uint32 arm_mem_base;
extern uint32 arm_mem_size;

extern uint32 videocore_base;
extern uint32 videocore_ceiling;
extern uint32 videocore_phys;
 
extern uint32 peripheral_base;
extern uint32 peripheral_ceiling;
extern uint32 peripheral_phys;
 
extern vm_addr user_base;
extern vm_addr user_ceiling;


/*
 *
 */


extern void *idle_process_table;
extern void *process_table;

extern int max_cpu;

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
extern struct Process *idle_process;

extern int max_process;




/*
 * Debugger
 */

extern volatile uint32 *gpio;
extern uint32 screen_width;
extern uint32 screen_height;
extern uint32 screen_buf;
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
 
extern int max_handle;
extern struct Handle *handle_table;
extern handle_list_t free_handle_list;



/*
 *
 */
 
extern uint32 *pagedirectory;
extern uint32 *pagetable_pool;
extern pagetable_list_t free_pagetable_list;

extern int max_virtseg;
extern struct VirtualSegment *virtseg_table;
extern int virtseg_cnt;

extern int max_physseg;
extern struct PhysicalSegment *physseg_table;
extern int physseg_cnt;

extern struct Rendez vm_rendez;

extern int64 segment_version_counter;

/*
 *
 */

extern int max_channel;
extern channel_list_t free_channel_list;
extern struct Channel *channel_table;

/*
 *
 */
 
extern int max_notification;
extern notification_list_t free_notification_list;
extern struct Notification *notification_table;



extern int max_parcel;
extern parcel_list_t free_parcel_list;
extern struct Parcel *parcel_table;

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









#endif

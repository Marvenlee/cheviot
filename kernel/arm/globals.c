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

/*
 * All kernel global variables are stored in this single file.
 */
 
#include <kernel/types.h>
#include <kernel/lists.h>
#include <kernel/proc.h>
#include <kernel/vm.h>
#include <kernel/arm/arm.h>
#include <kernel/arm/raspberry.h>



/*
 *
 */


spinlock_t inkernel_now;
int inkernel_lock;


int free_handle_cnt;
int free_process_cnt;
int free_channel_cnt;
int free_timer_cnt;
int free_isr_handler_cnt;
// int free_notification_cnt;




volatile struct bcm2835_system_timer_registers *timer_regs
                = (struct bcm2835_system_timer_registers *)ST_BASE;

volatile struct bcm2835_gpio_registers *gpio_regs
                = (struct bcm2835_gpio_registers *)GPIO_BASE;

volatile struct bcm2835_interrupt_registers *interrupt_regs
                = (struct bcm2835_interrupt_registers *)ARMCTRL_IC_BASE;


/*
 * ARM default state of registers
 */

bits32_t cpsr_dnm_state;



/*
 * Raspberry PI hardware addresses
 */

uint32 mailbuffer[64]  __attribute__ ((aligned (16)));
uint32 arm_mem_base;
uint32 arm_mem_size;

uint32 videocore_base;
uint32 videocore_ceiling;
uint32 videocore_phys;

uint32 peripheral_base;
uint32 peripheral_ceiling;
uint32 peripheral_phys;




/*
 * Process and Scheduler
 */

int max_cpu;
int cpu_cnt;
struct CPU cpu_table[MAX_CPU];

int max_process;
process_list_t free_process_list;
void *process_table;
void *idle_process_table;

struct Process *root_process;
struct Process *idle_task;
struct Process *vm_task;

bool reschedule_request;
process_circleq_t realtime_queue[32];
uint32 realtime_queue_bitmap;
process_list_t stride_queue;
int stride_queue_cnt;
int global_tickets;
int global_stride;
int64 global_pass;


/*
 * Debugger
 */

volatile uint32 *gpio;
uint32 screen_width;
uint32 screen_height;
uint32 screen_buf;
uint32 screen_pitch;

vm_addr debug_base;
vm_addr debug_ceiling;
bool __debug_enabled;


/*
 * Interrupts
 */

int max_isr_handler;
struct ISRHandler *isr_handler_table;
isrhandler_list_t free_isr_handler_list;
int irq_mask_cnt[NIRQ];
int irq_handler_cnt[NIRQ];
isrhandler_list_t isr_handler_list[NIRQ];
bits32_t mask_interrupts[3];
bits32_t pending_interrupts[3];


/*
 *
 */

struct BootInfo *bootinfo;
char *cfg_boot_prefix;
int cfg_boot_verbose;




/*
 * Handles
 */
 
int max_handle;
struct Handle *handle_table;
handle_list_t free_handle_list;




/*
 * Virtual Memory management
 */
 
/* 
 * The pagedirectory and pagetable_pool combined are stored within a 4MB area
 * of the kernel, starting at 4MB.  The page directory and page tables must
 * be in memory marked as uncacheable.
 *
 * Each process has a pmap structure to manage a process's pagetables.  Each
 * process is given 16 1k pagetables that are recyclable/discardable if there is
 * no free page table.  4MB allows for a maximum of 255 processes plus the
 * page directory.
 *
 * A possible option is to use a 1MB pool of 1024 page tables and share them
 * among all processes, but limiting each process to a maximum of 16 page tables
 * at a time. Either 63 pools of 16k or manage it by maintaining Pmap descriptors for
 * each page table, attaching them to the owner pmap..
 * 
 * LRU tables based on least recently run process, on task switch move to tail of
 * list.
 */
 
 
struct Pmap pmap_table[NPMAP];
pmap_list_t pmap_lru_list;
uint32 *pagedirectory;
uint32 *phys_pagedirectory;
vm_addr pagetable_base;

vm_size free_segment_size[NSEGBUCKET];

seg_list_t free_segment_list[NSEGBUCKET];

seg_list_t segment_heap;

seg_list_t cache_lru_list;
seg_list_t cache_hash[CACHE_HASH_SZ];

struct Segment *last_aged_seg;

int max_seg;                            // Maximum size of vseg_table
struct Segment *seg_table;              // Virtual memory map of single address space
int seg_cnt;                            // Current size of vseg_table

int64 next_unique_segment_id;           // Next unique identifier for segments


struct Rendez compact_rendez;
struct Rendez alloc_rendez;
struct Rendez vm_task_rendez;




vm_addr memory_ceiling;
vm_size requested_alloc_size;
vm_size garbage_size;
vm_size cache_size;
vm_size min_cache_size;
vm_size max_cache_size;



struct Segment *compact_seg;
vm_size compact_offset;










vm_addr user_base;
vm_addr user_ceiling;
uint32 heap_base;
uint32 heap_ceiling;
uint32 heap_ptr;



/*
 * Channel
 */

int max_channel;                    // Maximum number of IPC Channels
channel_list_t free_channel_list;   // List of free Channels
struct Channel *channel_table;      // Array of Channels


/*
 * Notification
 */

// int max_notification;
// notification_list_t free_notification_list;
// struct Notification *notification_table;



/*
 *
 */
 
int max_parcel;
parcel_list_t free_parcel_list;
struct Parcel *parcel_table;



/*
 * Timer
 */

timer_list_t timing_wheel[JIFFIES_PER_SECOND];
timer_list_t free_timer_list;
int max_timer;
struct Timer *timer_table;

volatile spinlock_t timer_slock;

volatile long long hardclock_seconds;
volatile long hardclock_jiffies;
volatile long long softclock_seconds;
volatile long softclock_jiffies;







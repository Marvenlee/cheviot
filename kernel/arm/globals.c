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





/*
 * ARM default state of registers
 */

bits32_t cpsr_dnm_state;



/*
 * Raspberry PI hardware addresses
 */

uint32 mailbuffer[64]  __attribute__ ((aligned (16)));







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

uint32 screen_width;
uint32 screen_height;
void *screen_buf;
uint32 screen_pitch;

//volatile void *screen_buf;
volatile struct bcm2835_system_timer_registers *timer_regs;
volatile struct bcm2835_gpio_registers *gpio_regs;
volatile struct bcm2835_interrupt_registers *interrupt_regs;


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


 
int max_handle;
struct Handle *handle_table;
handle_list_t free_handle_list;


int max_pagecache;
pagecache_list_t free_pagecache_list;
pagecache_list_t lru_pagecache_list;
pagecache_list_t pagecache_hash[NPAGECACHE_HASH];
struct Pagecache *pagecache_table;

vm_size mem_size;
int max_pageframe;
struct Pageframe *pageframe_table;

vm_addr memory_ceiling;
vm_size requested_alloc_size;
vm_size garbage_size;
vm_size cache_size;
vm_size min_cache_size;
vm_size max_cache_size;


pageframe_list_t    free_4k_pf_list;
pageframe_list_t    free_16k_pf_list;
pageframe_list_t    free_64k_pf_list;

//uint32              free_pageframe_cnt;



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







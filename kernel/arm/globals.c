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
int free_notification_cnt;




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
struct CPU cpu_table[MAX_CPU];

int max_process;
process_list_t free_process_list;
void *process_table;
void *idle_process_table;

struct Process *root_process;
struct Process *idle_task;
struct Process *vm_task;
struct Process *reaper_task;

struct Process *memory_daemon;
struct Process *reaper_daemon;



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
 
uint32 *pagedirectory;
uint32 *pagetable_pool;
pagetable_list_t free_pagetable_list;

int max_vseg;
struct VirtualSegment *vseg_table;
int vseg_cnt;

int max_pseg;
struct PhysicalSegment *pseg_table;
int pseg_cnt;

struct Rendez vm_rendez;

int64 segment_version_counter;




vm_addr user_base;
vm_addr user_ceiling;
uint32 heap_base;
uint32 heap_ceiling;
uint32 heap_ptr;



/*
 * Channel
 */

int max_channel;
channel_list_t free_channel_list;
struct Channel *channel_table;


/*
 * Notification
 */

int max_notification;
notification_list_t free_notification_list;
struct Notification *notification_table;



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

volatile long long hardclock_seconds;
volatile long hardclock_jiffies;
volatile long long softclock_seconds;
volatile long softclock_jiffies;







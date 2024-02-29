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
 * free counters (unused?)
 */
int free_pid_cnt;
int free_handle_cnt;
int free_process_cnt;
int free_timer_cnt;

/*
 * Processes and CPUs
 */
int max_cpu;
int cpu_cnt;
struct CPU cpu_table[MAX_CPU];

int max_process;
struct Process *process_table;
struct Process *root_process;

/*
 * Scheduler
 */
process_circleq_t sched_queue[32];
uint32_t sched_queue_bitmap;
int bkl_locked;
spinlock_t inkernel_now;
int inkernel_lock;
struct Process *bkl_owner;
process_list_t bkl_blocked_list;

/*
 * Interrupt handling
 */

int irq_mask_cnt[NIRQ];
int irq_handler_cnt[NIRQ];
isr_handler_list_t isr_handler_list[NIRQ];

struct Process *interrupt_dpc_process;
int max_isr_handler;
struct ISRHandler *isr_handler_table;
isr_handler_list_t isr_handler_free_list;
struct Rendez interrupt_dpc_rendez;
isr_handler_list_t pending_isr_dpc_list;


/*
 * Timer
 */
struct Process *timer_process;
timer_list_t timing_wheel[JIFFIES_PER_SECOND];
struct Rendez timer_rendez;

volatile spinlock_t timer_slock;
volatile long hardclock_time;
volatile long softclock_time;

// timer_list_t free_timer_list;
// int max_timer;
// struct Timer *timer_table;


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

#include <kernel/board/arm.h>
#include <kernel/board/boot.h>
#include <kernel/board/raspberry.h>
#include <kernel/filesystem.h>
#include <kernel/lists.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/vm.h>
#include <kernel/kqueue.h>
#include <kernel/interrupt.h>

/*
 * Boot args
 */

struct BootInfo *bootinfo;
struct BootInfo bootinfo_kernel;

char *cfg_boot_prefix;
int cfg_boot_verbose;

/*
 * ARM default state of registers
 */

bits32_t cpsr_dnm_state;

/*
 * Debugger
 */

uint32_t screen_width;
uint32_t screen_height;
void *screen_buf;
uint32_t screen_pitch;

/*
 * Interrupts
 */

int irq_mask_cnt[NIRQ];
int irq_handler_cnt[NIRQ];
isr_handler_list_t isr_handler_list[NIRQ];

bits32_t mask_interrupts[3];
bits32_t pending_interrupts[3];
uint32_t *vector_table;

struct InterruptAPI interrupt_api = 
{
  .EventNotifyFromISR = knote_from_isr,
  .MaskInterrupt = MaskInterruptFromISR,            // increment mask count (so on return to user it remains masked).
  .UnmaskInterrupt = UnmaskInterruptFromISR,
  .context = NULL
};


volatile struct bcm2835_system_timer_registers *timer_regs;
volatile struct bcm2835_gpio_registers *gpio_regs;
volatile struct bcm2835_interrupt_registers *interrupt_regs;
volatile struct bcm2835_uart_registers *uart_regs;

/*
 *
 */

vm_addr _heap_base;
vm_addr _heap_current;

vm_addr debug_base;
vm_addr debug_ceiling;

vm_addr boot_base;
vm_addr boot_ceiling;

uint32_t *root_pagedir;
uint32_t *io_pagetable;
uint32_t *cache_pagetable;



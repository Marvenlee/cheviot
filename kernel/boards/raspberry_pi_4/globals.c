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
#include <kernel/board/interrupt.h>
#include <kernel/board/timer.h>
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
 * Interrupts
 */
bits32_t mask_interrupts[3];
bits32_t pending_interrupts[3];
uint32_t *vector_table;

struct InterruptAPI interrupt_api = 
{
  .EventNotifyFromISR = interruptapi_knotei,
  .MaskInterrupt = interruptapi_mask_interrupt,
  .UnmaskInterrupt = interruptapi_unmask_interrupt,
  .context = NULL
};


/*
 * Peripheral addresses
 */
struct bcm2711_timer_registers *timer_regs;
struct bcm2711_gpio_registers *gpio_regs;
struct bcm2711_aux_registers *aux_regs;
struct bcm2711_gic_dist_registers *gic_dist_regs;
struct bcm2711_gic_cpu_iface_registers *gic_cpu_iface_regs;


/*
 *
 */
vm_addr _heap_base;
vm_addr _heap_current;

vm_addr core_pagetable_base;
vm_addr core_pagetable_ceiling;

vm_addr boot_base;
vm_addr boot_ceiling;

uint32_t *root_pagedir;
uint32_t *io_pagetable;
uint32_t *cache_pagetable;

uint32_t *mailbuffer;
uint32_t *mailbuffer_pa;



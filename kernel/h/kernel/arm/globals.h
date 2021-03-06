#ifndef KERNEL_ARM_GLOBALS_H
#define KERNEL_ARM_GLOBALS_H

#include <kernel/arm/arm.h>
#include <kernel/arm/boot.h>
#include <kernel/arm/raspberry.h>
#include <kernel/arm/task.h>
#include <kernel/filesystem.h>
#include <kernel/lists.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/vm.h>


/*
 * Debugger
 */

extern uint32_t screen_width;
extern uint32_t screen_height;
extern void *screen_buf;
extern uint32_t screen_pitch;

/*
 * Boot args
 */

extern struct BootInfo *bootinfo;
extern struct BootInfo bootinfo_kernel;

extern char *cfg_boot_prefix;
extern int cfg_boot_verbose;

/*
 * ARM default state of registers
 */

extern bits32_t cpsr_dnm_state;

/*
 * Interrupts
 */

extern int irq_mask_cnt[NIRQ];
extern int irq_handler_cnt[NIRQ];
extern vnode_list_t isr_handler_list[NIRQ];
extern bits32_t mask_interrupts[3];
extern bits32_t pending_interrupts[3];
extern uint32_t *vector_table;

extern volatile struct bcm2835_system_timer_registers *timer_regs;
extern volatile struct bcm2835_gpio_registers *gpio_regs;
extern volatile struct bcm2835_interrupt_registers *interrupt_regs;
extern volatile struct bcm2835_uart_registers *uart_regs;

/*
 *
 */

extern vm_addr _heap_base;
extern vm_addr _heap_current;

extern vm_addr debug_base;
extern vm_addr debug_ceiling;

extern vm_addr root_base;
extern vm_addr root_ceiling;

extern uint32_t *root_pagedir;
extern uint32_t *io_pagetable;
extern uint32_t *cache_pagetable;

/*
 * Boot Image File System
 */

extern void *ifs_image;
extern vm_size ifs_image_size;

#endif

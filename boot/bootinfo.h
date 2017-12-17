#ifndef BOOTINFO_H
#define BOOTINFO_H

#include "types.h"


struct BootInfo
{
	void (*root_entry_point)(void);
	void *root_stack_top;
	void *root_pagedir;
	void *kernel_pagetables;
	void *root_pagetables;
	void *io_pagetables;
	uint32 screen_width;
	uint32 screen_height;
	uint32 screen_pitch;
	void *screen_buf;
    void *timer_regs;
    void *gpio_regs;
    void *interrupt_regs;
    vm_size mem_size;
};


#endif

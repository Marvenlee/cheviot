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
 * Kernel initialization.
 */

#include <kernel/types.h>
#include <kernel/vm.h>
#include <kernel/proc.h>
#include <kernel/dbg.h>
#include <kernel/utility.h>
#include <kernel/arm/init.h>
#include <kernel/arch.h>
#include <kernel/globals.h>
#include <kernel/arm/boot.h>

// Globals

vm_addr _heap_base;
vm_addr _heap_current;

void KernelHeapInit (void);
void *KernelAlloc (vm_size size);
void InitProcesses(void);
void PopulateMemoryMap (void);


/*!
    Main();

    Main initialization routine of the kernel.  Called by Entry() in
    i386/asm/entry.S.  entry.S sets up a temporary stack and initialises
    the "mbi" pointer to point to the boot info structure.

    Init() is responsible for calling the initialization functions of
    all the OS subsystems.
    
    The global init_complete is used to prevent the KPRINTF/debugger
    from using spinlocks during kernel initialization as they depend
    on the CPU and Process structures to disable/enable preemption.
*/
void Main (void)
{
    screen_buf = (vm_addr)bootinfo->screen_buf;
    screen_width = bootinfo->screen_width;
    screen_height = bootinfo->screen_height;
    screen_pitch = bootinfo->screen_pitch;
    timer_regs = bootinfo->timer_regs;
    interrupt_regs = bootinfo->interrupt_regs;
    gpio_regs = bootinfo->gpio_regs;
    mem_size = bootinfo->mem_size;
    
    max_process = 256;
    max_handle = 128;
    
    max_timer = max_process * 2;
    max_isr_handler = 128;    
    max_pageframe = mem_size / PAGE_SIZE;
    max_pagecache = 1024;
 
    InitDebug ();    

    KLog ("Kernel Hello Again\n");

    KLog ("screen_buf = %08x", (vm_addr) screen_buf);
    KLog ("timer_regs = %08x", timer_regs);
    KLog ("interrupt_regs = %08x", interrupt_regs);
    KLog ("gpio_regs = %08x", gpio_regs);
    
    BlinkLEDs(10);

    KernelHeapInit();
        
    idle_process_table   = KernelAlloc (PROCESS_SZ * MAX_CPU);
    process_table        = KernelAlloc (PROCESS_SZ * max_process);
    pageframe_table      = KernelAlloc (sizeof (struct Pageframe) * max_pageframe);
    pagecache_table      = KernelAlloc (sizeof (struct Pagecache) * max_pagecache);   
    timer_table          = KernelAlloc (max_timer       * sizeof (struct Timer));
    isr_handler_table    = KernelAlloc (max_isr_handler * sizeof (struct ISRHandler));
    handle_table         = KernelAlloc (max_handle      * sizeof (struct Handle));

    InitArm();

    // InitVM calls a callback to populate the memory map

    InitVM();
 
    
/* pass bootinfo to InitProcesses()
    root_entry_point = bootinfo->root_entry_point;
    root_stack_ceiling = bootinfo->root_task_stack_top;    
    root_pagedir = bootinfo->root_pagedir;
*/    
 
    InitProcesses();
    StartRootProcess();
}



/*!
*/
void KernelHeapInit (void)
{   
    _heap_base = ALIGN_UP ((vm_addr)&_ebss, 65536);
    _heap_current = _heap_base;

    KLog ("heap base >> va = %08x\n", _heap_base);
}


/*!
*/
void *KernelAlloc (vm_size sz)
{    
    vm_addr va = _heap_current;

    sz = ALIGN_UP (sz, PAGE_SIZE);    
    _heap_current += ALIGN_UP (sz, PAGE_SIZE);
    return (void *)va;
}



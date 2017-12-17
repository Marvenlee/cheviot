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

#include "types.h"
#include "dbg.h"
#include "arm.h"
#include "bootinfo.h"
#include "memory.h"
#include "globals.h"
#include "bootstrap.h"


// Constants
#define ROOT_CEILING_ADDR               0x00077000

#define IO_PAGETABLES_ADDR              0x00077000
#define IO_PAGETABLES_CNT               16
#define IO_PAGETABLES_PDE_BASE          2560

#define ROOT_PAGETABLES_ADDR            0x0007B000
#define ROOT_PAGETABLES_CNT             1
#define ROOT_PAGETABLES_PDE_BASE        0

#define ROOT_PAGEDIR_ADDR               0x0007C000

#define KERNEL_PAGETABLES_ADDR          0x00080000
#define KERNEL_PAGETABLES_CNT           512
#define KERNEL_PAGETABLES_PDE_BASE      2048
#define KERNEL_ADDR                     0x80000000


// Prototypes
extern uint32 GetCtrl(void);
extern void SetCtrl (uint32 ctrl);
extern void EnablePaging(uint32 *pagedir, uint32 flags);
void InitPageDirectory (void);
void InitKernelPagetables (void);
void InitRootPagetables (void);
void InitIOPagetables (void);    
void *IOMap (vm_addr pa, size_t sz);


uint32 *io_pagetables;
uint32 *root_pagetables;
uint32 *root_pagedir;
uint32 *kernel_pagetables;
//struct bcm2835_system_timer_registers *timer_regs_va;
//struct bcm2835_gpio_registers *gpio_regs_va;
//struct bcm2835_interrupt_registers *interrupt_regs_va;
vm_addr iomap_base;
vm_addr iomap_current;


/*!
    Pulls the kernel up by its bootlaces.
    
    Kernel is loaded at 1MB (0x00100000) but is compiled to run at 0x80100000.
    Sets up pagetables to map physical memory starting at 0 to 512MB into kernel
    starting at 2GB (0x80000000).

    IO memory for screen, timer, interrupt and gpios are mapped at 0xA0000000.    
    Bootloader and loaded modules are identity mapped from 4k upto 478k.
*/
void BootstrapKernel (void)
{
    io_pagetables     = (uint32 *) IO_PAGETABLES_ADDR;      // 478k  16k for IO mapped areas, screen, timer, interrupt, gpio
    root_pagetables   = (uint32 *) ROOT_PAGETABLES_ADDR;    // 494k  (4k for root pagetable)  Only needs 4k  1k + 3k VPTE
    root_pagedir      = (uint32 *) ROOT_PAGEDIR_ADDR;       // 498k  (16k page directory)
    kernel_pagetables = (uint32 *) KERNEL_PAGETABLES_ADDR;  // 512k  (512k identity mapped memory)
    // Leave to kernel to allocate page table pool for page cache.

    // TODO: Add pagetable/pagedir "virtual kernel" addresses to bootinfo and sizes.
    
    KLog ("BootstrapKernel");

    bootinfo.root_pagedir = (void *) ((vm_addr)root_pagedir + KERNEL_ADDR);
    bootinfo.kernel_pagetables = (void *) ((vm_addr)kernel_pagetables + KERNEL_ADDR);
    bootinfo.io_pagetables = (void *) ((vm_addr)io_pagetables + KERNEL_ADDR);
    bootinfo.root_pagetables = (void *) ((vm_addr)root_pagetables + KERNEL_ADDR);

    InitPageDirectory();
    KLog ("InitPageDirectory DONE");

    InitKernelPagetables();
    KLog ("InitKernelPagetables DONE");

    InitRootPagetables();
    KLog ("InitRootPagetables DONE");

    InitIOPagetables();    
    KLog ("InitIOPagetables DONE");

    // Set ARM v5 page table format
    uint32 ctrl = GetCtrl();
    ctrl &= ~C1_XP;
    SetCtrl (ctrl);        

    KLog ("EnablePaging");
        
    EnablePaging (root_pagedir, 0x00800001);
}


/*!
    Initialises the root page directory. Root and IO pagetables have entries marked
    as not present and a partially populated later.
*/
void InitPageDirectory(void)
{
    int t;
    uint32 *phys_pt;

    for (t=0; t < N_PAGEDIR_PDE; t++)
    {
        root_pagedir[t] = L1_TYPE_INV;
    }

    for (t=0; t < KERNEL_PAGETABLES_CNT; t++)
    {
        phys_pt = kernel_pagetables + t * N_PAGETABLE_PTE;
        root_pagedir[KERNEL_PAGETABLES_PDE_BASE + t] = (uint32)phys_pt | L1_TYPE_C;
    }

    for (t=0; t < IO_PAGETABLES_CNT; t++)    
    {
        phys_pt = io_pagetables + t * N_PAGETABLE_PTE;
        root_pagedir[IO_PAGETABLES_PDE_BASE + t] = (uint32)phys_pt | L1_TYPE_C;
    }

    for (t = 0; t < ROOT_PAGETABLES_CNT * N_PAGETABLE_PTE; t++)
    {
        root_pagetables[t] = L2_TYPE_INV;
    }

    for (t = 0; t < IO_PAGETABLES_CNT * N_PAGETABLE_PTE; t++)
    {
        io_pagetables[t] = L2_TYPE_INV;
    }
}


/*!
    Initialise the page table entries to map phyiscal memory from 0 to 512MB into the
    kernel starting at 0x80000000.
*/
void InitKernelPagetables (void)
{    
    uint32 pa_bits;
    vm_addr pa;
        
    pa_bits = L2_TYPE_S | L2_AP(AP_W);  // TODO: Enable caching with  | L2_B | L2_C;

    for (pa = 0; pa < bootinfo.mem_size; pa += PAGE_SIZE)
    {        
        kernel_pagetables[pa/PAGE_SIZE] = pa | pa_bits;
    }
}


/*!
    Populate the page table for the root process (this bootloader).
    Identity maps memory from 4k to 478k.  In the kernel user processes
    use 4k pagetables with 1k being the actual hardware defined pagetable
    and the remaining 3k holding virtual-PTEs, for each page table entry.
*/
void InitRootPagetables (void)
{
    uint32 pa_bits;
    vm_addr pa;
            
    root_pagedir[0] = (uint32)root_pagetables | L1_TYPE_C;
    
    for (pa = 0; pa < ROOT_CEILING_ADDR; pa+= PAGE_SIZE)
    {
        // TODO: Enable caching with  | L2_B | L2_C;
        pa_bits = L2_TYPE_S | L2_AP(AP_W);
        
        if (pa > 0) {
            pa_bits |= L2_AP (AP_U);
        }
        
        // Remember, 1k for ptes, 3k for vptes per 4k pagetable
        // Don't forget to initialize, clear vptes

        root_pagetables[pa / PAGE_SIZE] = pa | pa_bits;
    }
}


/*!
    Populate the IO pagetables at 0x80000000
*/
void InitIOPagetables (void)
{    
    iomap_base = IOMAP_BASE_VA;
    iomap_current = iomap_base;
        
    bootinfo.screen_buf = IOMap ((vm_addr)screen_buf, screen_pitch * screen_height);        
    bootinfo.timer_regs = IOMap (ST_BASE, sizeof (struct bcm2835_system_timer_registers));
    bootinfo.interrupt_regs = IOMap (ARMCTRL_IC_BASE, sizeof (struct bcm2835_interrupt_registers));
    bootinfo.gpio_regs = IOMap (GPIO_BASE, sizeof (struct bcm2835_gpio_registers));

    KLog ("screen_buf = %08x", bootinfo.screen_buf);
    KLog ("timer_regs = %08x", bootinfo.timer_regs);
    KLog ("interrupt_regs = %08x", bootinfo.interrupt_regs);
    KLog ("gpio_regs = %08x", bootinfo.gpio_regs);
}


/*!
    Map a region of memory into the IO region above 0xA0000000.
*/
void *IOMap (vm_addr pa, size_t sz)
{
    int pte_idx;
    uint32 pa_bits;
    vm_addr pa_base, pa_ceiling;    
    vm_addr va_base, va_ceiling;
    vm_addr regs;
    vm_addr paddr;
    
    pa_base = ALIGN_DOWN (pa, PAGE_SIZE);
    pa_ceiling = ALIGN_UP (pa + sz, PAGE_SIZE);

    va_base = ALIGN_UP (iomap_current, PAGE_SIZE);
    va_ceiling = va_base + (pa_ceiling - pa_base);    
    regs = (va_base + (pa - pa_base));
    
    pa_bits = L2_TYPE_S | L2_AP(AP_W); // TODO:  Any way to make write-combine/write thru ?

    paddr = pa_base;
    pte_idx = (va_base - iomap_base) / PAGE_SIZE;

    for (paddr = pa_base; paddr < pa_ceiling; paddr += PAGE_SIZE)
    {
        io_pagetables[pte_idx] = paddr | pa_bits;
        pte_idx ++;
    }

    iomap_current = va_ceiling;

    return (void *)regs;
} 



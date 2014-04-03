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
 * Memory Managment intitialization and enabling of paging.
 */
 

#include <kernel/vm.h>
#include <kernel/proc.h>
#include <kernel/dbg.h>
#include <kernel/utility.h>
#include <kernel/arch.h>
#include <kernel/arm/init.h>
#include <kernel/arm/boot.h>
#include <kernel/globals.h>


extern void start_mmu ( unsigned int, unsigned int );
void GetVideoCoreMem (uint32 *phys_base, uint32 *size);




/*
 * InitVM();
 *
 * Called by Init() in i386/init/main.c
 */

void InitVM (void)
{
    uint32 t, u;
    vm_addr paddr, vaddr;
    uint32 videocore_size;
    struct PmapMem *pmap_mem;
    bits32_t ctrl;   
    
    KLog ("InitVM()");
    
//  Calculate table sizes
    
    max_segment = 32000;
    max_memarea = 32000;
    max_process = 128;
    max_timer   = 1024;
    max_isr_handler = 128;
    max_channel = 10000;
    max_notification = 4000;
    max_handle  = 10000;

    user_base = bootinfo->user_base;
    user_ceiling = 0x80000000;

    heap_base = bootinfo->heap_base;
    heap_ceiling = bootinfo->heap_ceiling;
    heap_ptr = heap_base;

//  MemSet ((void *)heap_base, 0, heap_ceiling - heap_base);
    
    KLog ("...allocating heap");
    
    idle_process_table   = HeapAlloc (MAX_CPU         * PROCESS_SZ);
    process_table        = HeapAlloc (max_process     * PROCESS_SZ);
    timer_table          = HeapAlloc (max_timer       * sizeof (struct Timer));
    isr_handler_table    = HeapAlloc (max_isr_handler * sizeof (struct ISRHandler));
    channel_table        = HeapAlloc (max_channel     * sizeof (struct Channel));
    notification_table   = HeapAlloc (max_notification * sizeof (struct Notificationl));    
    handle_table         = HeapAlloc (max_handle      * sizeof (struct Handle));
    segment_table        = HeapAlloc (max_segment     * sizeof (struct Segment));
    memarea_table        = HeapAlloc (max_memarea     * sizeof (struct MemArea));


    for (u = 0, t = 0; t < bootinfo->segment_cnt; t++)
    {
        if (bootinfo->segment_table[t].base >= bootinfo->user_base)
        {
            segment_table[u].base    =  bootinfo->segment_table[t].base;
            segment_table[u].ceiling =  bootinfo->segment_table[t].ceiling;
            segment_table[u].type    =  bootinfo->segment_table[t].type;
            
            memarea_table[u].base          = bootinfo->segment_table[t].base;
            memarea_table[u].ceiling       = bootinfo->segment_table[t].ceiling;
            memarea_table[u].physical_addr = bootinfo->segment_table[t].base;
            memarea_table[u].owner_process = NULL;
            memarea_table[u].msg_handle = -1;
                
            if (segment_table[u].type == SEG_TYPE_ALLOC)
                memarea_table[u].flags = MEM_ALLOC | bootinfo->segment_table[t].flags;
            else if (segment_table[u].type == SEG_TYPE_FREE)
                memarea_table[u].flags = MEM_FREE;
            else if (segment_table[u].type == SEG_TYPE_RESERVED)
                memarea_table[u].flags = MEM_RESERVED;
            else
                KernelPanic ("Undefined bootinfo segment");
        
            u++;
        }
    }

    memarea_table[u-1].ceiling = user_ceiling;
    segment_cnt = u;
    memarea_cnt = u;
    memarea_version_counter = 0;
    
    KASSERT (memarea_cnt > 0);
    
    
    // FIXME: Move up to beginning of function?
    
    videocore_base = 0x80000000;
    GetVideoCoreMem(&videocore_phys, &videocore_size);
    videocore_ceiling = videocore_base + videocore_size;    
    
    peripheral_base = 0xd0000000; 
    peripheral_ceiling = 0xf0000000;
    peripheral_phys = 0x20000000;
    
    KLOG ("heap_ptr = %#010x", heap_ptr);
    KASSERT (heap_ptr <= 0x00400000);
    
    pagedirectory  = (uint32*)0x00400000;       // 16K page directory
    pagetable_pool = (uint32*)0x00404000;       // page table pool above 16k page directory
    
    
    ctrl = GetCtrl();               // Set ARM v5 page table format
    ctrl &= ~C1_XP;
    SetCtrl (ctrl);
    
        
    MemSet (pagedirectory, 0, L1_TABLE_SIZE);
    
    LIST_INIT (&free_pagetable_list);
    
    for (t=0; t < max_process; t++)
    {
        pmap_mem = (struct PmapMem *)((uint8*)pagetable_pool + t * (L2_TABLE_SIZE * 16));
         
        LIST_ADD_TAIL (&free_pagetable_list, pmap_mem, free_link);
    }
        
    KLog ("Initializing page directory");
    
    for (paddr = 0, vaddr=0;
            vaddr < 0x00400000;
            paddr += 0x00100000, vaddr += 0x00100000)
        pagedirectory[vaddr>>20] = L1_TYPE_S | L1_S_AP(AP_KRW) | L1_S_B | L1_S_C | paddr;
    
    for (paddr = 0x00400000, vaddr=0x00400000;
            vaddr < 0x00800000;
            paddr += 0x00100000, vaddr += 0x00100000)
        pagedirectory[vaddr>>20] = L1_TYPE_S | L1_S_AP(AP_KRW) | paddr;
    
    
    
    for (paddr = videocore_phys, vaddr = videocore_base;
            vaddr < videocore_ceiling;
            paddr += 0x00100000, vaddr += 0x00100000)
        pagedirectory[vaddr>>20] = L1_TYPE_S | L1_S_AP(AP_KRW) | paddr;
    
    for (paddr = peripheral_phys, vaddr = peripheral_base;
            vaddr < peripheral_ceiling;
            paddr += 0x00100000, vaddr += 0x00100000)
        pagedirectory[vaddr>>20] = L1_TYPE_S | L1_S_AP(AP_KRW) | paddr;
    
    KLog ("*** screen_buf     = %#010x", screen_buf);
    KLog ("*** videocore_phys = %#010x", videocore_phys); 
    KLog ("Enabling paging");

    EnablePaging((uint32) pagedirectory, 0x00800001);
    
    screen_buf = videocore_base + (screen_buf - videocore_phys);

    KLog ("Hello from other side!!");
    KLog ("Double checking");
    KLog ("screen_buf = %#010x", screen_buf);

    InitRendez (&vm_rendez);
}








/*
 * BootAlloc();
 *
 * Allocate an area of physical/virtal memory for a kernel array.
 */

void *HeapAlloc (uint32 sz)
{
    void *p;
    
    p = (void *)heap_ptr;
    heap_ptr += ALIGN_UP (sz, PAGE_SIZE);

    return p;
}





/*
 *
 */

void GetVideoCoreMem (uint32 *phys_base, uint32 *size)
{
    volatile uint32 result;
    
    mailbuffer[0] = 8 * 4;
    mailbuffer[1] = 0;
    mailbuffer[2] = 0x00010006;
    mailbuffer[3] = 8;
    mailbuffer[4] = 0;
    mailbuffer[5] = 0;
    mailbuffer[6] = 0;
    mailbuffer[7] = 0;

    do
    {
        MailBoxWrite ((uint32)mailbuffer, 8);
        result = MailBoxRead (8);
    } while (result == 0);


    *phys_base = mailbuffer[5];
    *size = mailbuffer[6];
}




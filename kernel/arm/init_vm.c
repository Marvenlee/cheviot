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

int AllocateSeg (int i, vm_addr base, vm_size size, bits32_t flags);
int InitSegBucket (int i, int q, vm_addr base, vm_addr ceiling, vm_size size);




/*
 * Initializes the kernel's memory management, allocates tables in the
 * kernel for all kernel objects.  The kernel starts at 0MB and ends at 8MB.
 * User-Mode begins at 8MB and ends at 2GB.  Above 2GB the framebuffer and
 * Broadcom chipset peripheral memory are mapped.
 *
 * A memory map is passed by the bootloader in the bootinfo structure.
 * All structures and tables passed by the bootloader is guaranteed to be
 * below 1MB, so the kernel is mapped at 1MB.  The Executive (or root process)
 * is compiled to load at 8MB.
 *
 * Two tables manage virtual memory,  the virtseg_table manages the virtual
 * address space (remember it is a single address space) and the
 * physseg_table.manages physical memory.  These arrays are expanding arrays
 * but have a maximum size.
 */

void InitVM (void)
{
    uint32 t, i, u;
    vm_addr paddr, vaddr;
    uint32 videocore_size;
    bits32_t ctrl;   
    
    vm_addr segment_heap_start;
    
    
    KLog ("InitVM()");
    
    
    user_base = bootinfo->user_base;
    user_ceiling = 0x80000000;

    heap_base = bootinfo->heap_base;
    heap_ceiling = bootinfo->heap_ceiling;
    heap_ptr = heap_base;

    MemSet ((void *)heap_base, 0, heap_ceiling - heap_base);
    
    idle_process_table   = HeapAlloc (MAX_CPU         * PROCESS_SZ);
    process_table        = HeapAlloc (max_process     * PROCESS_SZ);
    timer_table          = HeapAlloc (max_timer       * sizeof (struct Timer));
    isr_handler_table    = HeapAlloc (max_isr_handler * sizeof (struct ISRHandler));
    channel_table        = HeapAlloc (max_channel     * sizeof (struct Channel));
    handle_table         = HeapAlloc (max_handle      * sizeof (struct Handle));
    seg_table            = HeapAlloc (max_seg         * sizeof (struct Segment));
    parcel_table         = HeapAlloc (max_parcel      * sizeof (struct Parcel));

    memory_ceiling = 0;


    i = 0;
//    top_of_heap = user_base;
    next_unique_segment_id = 0;
     
    for (t = 0; t < bootinfo->segment_cnt; t++)
    {
        if (bootinfo->segment_table[t].base >= bootinfo->user_base
            && bootinfo->segment_table[t].type == BSEG_ALLOC)
        {
            
/*            pseg = LIST_HEAD (&free_pseg_list);
            LIST_REM_HEAD (&free_pseg_list, link);
            LIST_ADD_TAIL (&heap_pseg_list, pseg, link);
            
            pseg->base = bootinfo->segment_table[t].base;
            pseg->size = bootinfo->segment_table[t].ceiling - bootinfo->segment_table[t].base;
            pseg->virtual_addr = pseg->base;
            
            i = AllocateVSeg (i, pseg, bootinfo->segment_table[t].flags);
*/
                
//            total_mem_sz += pseg->size;
//            top_of_heap = pseg->base + pseg->size;


            memory_ceiling = bootinfo->segment_table[t].ceiling;
        }                
    }
        
    segment_heap_start = user_base;         // FIXME: Set above user segments passed in bootinfo.
    
        
    i = InitSegBucket (i, 0, segment_heap_start,   0x08000000, 0x00001000);   // 4k    * 32768   128MB
    i = InitSegBucket (i, 1, 0x08000000,  0x28000000, 0x00010000);   // 64k   * 8192    512MB
    i = InitSegBucket (i, 2, 0x28000000,  0x48000000, 0x00040000);   // 256k  * 2048    512MB
    i = InitSegBucket (i, 3, 0x48000000,  0x50000000, 0x00100000);   // 1MB   * 128     128MB
    i = InitSegBucket (i, 4, 0x50000000,  0x60000000, 0x00400000);   // 4MB   * 64      256MB
    i = InitSegBucket (i, 5, 0x60000000,  0x80000000, 0x01000000);   // 16MB  * 32      512MB
        
    seg_cnt = i;
   

    
    videocore_base = 0x80000000;
    GetVideoCoreMem(&videocore_phys, &videocore_size);
    videocore_ceiling = videocore_base + videocore_size;    
    
    peripheral_base = 0xd0000000; 
    peripheral_ceiling = 0xf0000000;
    peripheral_phys = 0x20000000;
    
    KLOG ("heap_ptr = %#010x", heap_ptr);
    KASSERT (heap_ptr <= 0x00400000);
    
    

    
    /*
     * The pagedirectory and pagetable_pool (255 1k pagetables, 16 pagetables per
     * process allocated between 4MB and 8MB.
     */
    
    
    pagetable_base = 0x00700000;
    pagedirectory  = (uint32*)pagetable_base;
    
    ctrl = GetCtrl();                           // Set ARM v5 page table format
    ctrl &= ~C1_XP;
    SetCtrl (ctrl);
    
    for (t=0; t < 4096; t++)                    // Clear page directory
        *(pagedirectory + t) = L1_TYPE_INV;
    
    for (t=0; t < 4096; t++)                    // Clear physical mapped page directory
        *(phys_pagedirectory + t) = L1_TYPE_INV;
        
        
        
    LIST_INIT (&pmap_lru_list);
    
    for (t=0; t < NPMAP; t++)
    {
        pmap_table[t].addr = (uint32 *)(pagetable_base + 0x8000) + (t * L2_TABLE_SIZE * 16);
        pmap_table[t].lru = 0;
        pmap_table[t].map_type = PMAP_MAP_PHYSICAL;
        pmap_table[t].owner = NULL;
        
        for (u=0; u< NPDE; u++)
        {
            pmap_table[t].pde[u].idx = -1;
            pmap_table[t].pde[u].val = L1_TYPE_INV;
        }
        
        LIST_ADD_TAIL (&pmap_lru_list, &pmap_table[t], lru_link);
    }
  
   
    // FIXME: Can only make first 1MB read only or clear it once the interrupt table
    // is initialized.
    
    for (paddr = 0, vaddr=0; vaddr < 0x00700000;
         paddr += 0x00100000, vaddr += 0x00100000)
    {
        pagedirectory[vaddr>>20] = L1_TYPE_S | L1_S_AP(AP_KRW) | L1_S_B | L1_S_C | paddr;
    }
    
    paddr = 0x00700000;
    vaddr=0x00700000;
    pagedirectory[vaddr>>20] = L1_TYPE_S | L1_S_AP(AP_KRW) | paddr;
    
    for (paddr = videocore_phys, vaddr = videocore_base; vaddr < videocore_ceiling;
         paddr += 0x00100000, vaddr += 0x00100000)
    {
        pagedirectory[vaddr>>20] = L1_TYPE_S | L1_S_AP(AP_KRW) | paddr;
    }
    
    for (paddr = peripheral_phys, vaddr = peripheral_base; vaddr < peripheral_ceiling;
         paddr += 0x00100000, vaddr += 0x00100000)
    {
        pagedirectory[vaddr>>20] = L1_TYPE_S | L1_S_AP(AP_KRW) | paddr;
    }


    for (paddr = 0, vaddr=0; vaddr < 0x80000000;
         paddr += 0x00100000, vaddr += 0x00100000)
    {
        phys_pagedirectory[vaddr>>20] = L1_TYPE_S | L1_S_AP(AP_KRW) | L1_S_B | L1_S_C | paddr;
    }
    

    
    KLog ("Enabling paging");

    EnablePaging((uint32) pagedirectory, 0x00800001);
    
    screen_buf = videocore_base + (screen_buf - videocore_phys);

    KLog ("Hello from other side!!");
    
//    InitRendez (&vm_busy_rendez);
}




/*
 *
 */
 
int AllocateSeg (int i, vm_addr base, vm_size size, bits32_t flags)
{
/*
    vseg_table[i].bucket_q = -1;
    vseg_table[i].base = pseg->base;
    vseg_table[i].ceiling = pseg->base + pseg->size;
    vseg_table[i].size = pseg->size;
    vseg_table[i].owner = NULL;
    vseg_table[i].flags = MEM_ALLOC | flags;
    vseg_table[i].physical_addr = pseg->base;
    vseg_table[i].physical_segment = pseg;
    vseg_table[i].version = segment_version_counter ++;    
    vseg_table[i].next_free = NULL;
 */
    
    return i+1;
}




/*
 *
 */

int InitSegBucket (int i, int q, vm_addr base, vm_addr ceiling, vm_size size)
{
    vm_addr addr;
    
    free_segment_size[q] = size;

    HEAP_INIT (&free_segment_list[q]);
    
    for (addr = base; addr < ceiling; addr += size, i++)
    {
        seg_table[i].base = addr;
        seg_table[i].bucket_q = q;
        seg_table[i].size = size;
        seg_table[i].owner = NULL;
        seg_table[i].flags = MEM_FREE;
        seg_table[i].physical_addr = 0;
        seg_table[i].segment_id = 0;
        
        HEAP_ADD_HEAD (&free_segment_list[q], seg_table, link);
    }
    
    return i;
}




/*
 * HeapAlloc();
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




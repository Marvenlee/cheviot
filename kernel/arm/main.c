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




/*
 * Init();
 *
 * Main initialization routine of the kernel.  Called by Entry() in
 * i386/asm/entry.S.  entry.S sets up a temporary stack and initialises
 * the "mbi" pointer to point to the boot info structure.
 *
 * Init() is responsible for calling the initialization functions of
 * all the OS subsystems.
 *
 * The global init_complete is used to prevent the KPRINTF/debugger
 * from using spinlocks during kernel initialization as they depend
 * on the CPU and Process structures to disable/enable preemption.
 */
 
void Main (void)
{
//  GetOptions();

//    max_pseg = 10000;
    max_seg = 10000;
    max_process = 128;
    max_timer   = 1024;
    max_isr_handler = 128;
    max_channel = 10000;
//    max_notification = 4000;
    max_handle  = 10000;
    max_parcel = max_handle + max_seg;
        
    free_process_cnt     = max_process;
    free_timer_cnt       = max_timer;
    free_isr_handler_cnt = max_isr_handler;
    free_channel_cnt     = max_channel;
    free_handle_cnt      = max_handle;
        
    InitDebug();

    KLog ("Kernel says Hello!");
    KLog ("bootinfo = %#010x", bootinfo);
    
    KLog ("heap_base = %#010x", bootinfo->heap_base);
    KLog ("heap_ceiling = %#010x", bootinfo->heap_ceiling);

    KLog ("segment_cnt = %d", bootinfo->segment_cnt);
    KLog ("width = %d", bootinfo->screen_width);
    KLog ("height = %d", bootinfo->screen_height);
    KLog ("pitch = %d", bootinfo->screen_pitch);
    KLog ("buf = %#010x", bootinfo->screen_buf);

    InitVM();

    KLog ("... Calling InitArm()");

    InitArm();
    
    KLog ("... Calling InitProc()");
    
    InitProc();
    

    KLog ("** Starting ROOT, Entry Point = %#010x", bootinfo->procman_entry_point);

    StartRootProcess();
    
    KLog ("** BACK TO KERNEL MAIN");
    
    while (1);
}










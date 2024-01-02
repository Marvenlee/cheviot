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

#define KDEBUG

#include <kernel/arch.h>
#include <kernel/board/boot.h>
#include <kernel/board/globals.h>
#include <kernel/board/init.h>
#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>
#include <kernel/kqueue.h>
#include <string.h>


/* @brief Entry point into the kernel
 *
 * The kernel is mapped at 0x80100000 along with the bootloader and page tables
 * The bootloader is mapped at 0x00008000 (32K)
 * The kernel's physical address starts at 0x00100000 (1MB)
 * Tootloader supplies initial pagetables.  Located above kernel.
 * The IFS image was mapped to the top of memory by the bootloader.
 *
 * Root process uses it as template for kernel PTEs.
 *
 * Bootinfo copied into kernel.  Shouldn't be any data below 1MB.
 * In init page tables, don't care if bootloader is mapped or not.
 * Only want to reserve pages for the IFS at the top of RAM
 * InitVM reserve pages for IFS image, marks as in use
 * Need to read from IFS image, VirtualAlloc and copy segments from image
 *
 * Kernel marks bootloader pages below 1MB as free.
 * 
 * Kernel creates the root process and initially populates it with the IFS image.
 * The kernel loads the IFS executable ELF sections into the root process by
 * reading the IFS image in the root process's address space.
 * The kernel allocates the stack and passes the virtual base and size of the IFS image.
 *
 * IFS process is the root process, it forks to create the process that handles
 * the IFS mount.
 */
void Main(void)
{
  memcpy(&bootinfo_kernel, bootinfo, sizeof bootinfo_kernel);
  bootinfo = &bootinfo_kernel;

  mem_size = bootinfo->mem_size;

  max_process = NPROCESS;
  max_pageframe = mem_size / PAGE_SIZE;
  max_buf = 4 * 1024;
  max_superblock = NR_SUPERBLOCK;
  max_filp = NR_FILP;
  max_vnode = NR_VNODE;
  max_pipe = NR_PIPE;
  max_kqueue = NR_KQUEUE;
  max_knote = NR_KNOTE;
  max_isr_handler = NR_ISR_HANDLER;
  
  init_bootstrap_allocator();

  io_pagetable      = bootstrap_alloc(4096);
  cache_pagetable   = bootstrap_alloc(256 * 1024);  
  vector_table      = bootstrap_alloc(PAGE_SIZE);
  pageframe_table   = bootstrap_alloc(max_pageframe * sizeof(struct Pageframe));
  buf_table         = bootstrap_alloc(max_buf * sizeof(struct Buf));
  pipe_table        = bootstrap_alloc(max_pipe * sizeof (struct Pipe));
  process_table     = bootstrap_alloc(max_process * PROCESS_SZ);
  superblock_table  = bootstrap_alloc(max_superblock * sizeof(struct SuperBlock));
  filp_table        = bootstrap_alloc(max_filp * sizeof(struct Filp));
  vnode_table       = bootstrap_alloc(max_vnode * sizeof(struct VNode));
  kqueue_table      = bootstrap_alloc(max_kqueue * sizeof(struct KQueue));
  knote_table       = bootstrap_alloc(max_knote * sizeof(struct KNote));
  isr_handler_table = bootstrap_alloc(max_isr_handler * sizeof(struct ISRHandler));
  
  init_io_pagetables();
  init_buffer_cache_pagetables();
  InitDebug();
  
  Info("Hello from kernel!\n");
  
  init_arm();
  init_vm();
  init_vfs();
  init_processes();

  Error("InitProcesses failed");
  while (1) {
  }
}


/* @brief   Initialize the bootstrap heap pointer
 */
void init_bootstrap_allocator(void)
{
  _heap_base = ALIGN_UP((vm_addr)bootinfo->pagetable_ceiling, 16384);
  _heap_current = _heap_base;
}


/* @brief   Allocate a contiguous area of memory in the kernel
 * 
 * @param   sz, allocation size in bytes
 * @return  pointer to area allocated
 */
void *bootstrap_alloc(vm_size sz)
{
  vm_addr va = _heap_current;

  sz = ALIGN_UP(sz, PAGE_SIZE);
  _heap_current += ALIGN_UP(sz, PAGE_SIZE);
  return (void *)va;
}


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

#define KDEBUG

#include <kernel/arch.h>
#include <kernel/arm/boot.h>
#include <kernel/arm/globals.h>
#include <kernel/arm/init.h>
#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>

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
void Main(void) {
  MemCpy(&bootinfo_kernel, bootinfo, sizeof bootinfo_kernel);
  bootinfo = &bootinfo_kernel;

  mem_size = bootinfo->mem_size;

  max_process = NPROCESS;
  max_pageframe = mem_size / PAGE_SIZE;
  max_buf = 4 * 1024;
  max_superblock = NR_SUPERBLOCK;
  max_filp = NR_FILP;               // Can only be 32k-1 max
  max_vnode = NR_VNODE;
  max_poll = NR_POLL;
  max_pipe = NR_PIPE;
  
  KernelHeapInit();

  io_pagetable = KernelAlloc(4096);
  cache_pagetable = KernelAlloc(256 * 1024);  
  vector_table = KernelAlloc(PAGE_SIZE);
  pageframe_table = KernelAlloc(max_pageframe * sizeof(struct Pageframe));
  buf_table = KernelAlloc(max_buf * sizeof(struct Buf));
  pipe_table = KernelAlloc(max_pipe * sizeof (struct Pipe));
  pipe_mem_base = KernelAlloc(max_pipe * PIPE_BUF_SZ);
  process_table = KernelAlloc(max_process * PROCESS_SZ);
  superblock_table = KernelAlloc(max_superblock * sizeof(struct SuperBlock));
  filp_table = KernelAlloc(max_filp * sizeof(struct Filp));
  vnode_table = KernelAlloc(max_vnode * sizeof(struct VNode));
  poll_table = KernelAlloc(max_poll * sizeof(struct Poll));

  InitIOPagetables();
  InitBufferCachePagetables();

  InitDebug();
  
  Info("Hello from kernel!\n");
  
  InitArm();
  
  Info("InitArm ok");
  
  InitVM();

  Info("InitVM ok");

  InitVFS();

  Info("InitVFS ok");
  
  InitProcesses();

  Info("InitProcesses failed");

  while (1) {
  }
}

/*
 *
 */
void KernelHeapInit(void) {
  _heap_base = ALIGN_UP((vm_addr)bootinfo->pagetable_ceiling, 16384);
  _heap_current = _heap_base;
}

/*
 *
 */
void *KernelAlloc(vm_size sz) {
  vm_addr va = _heap_current;

  sz = ALIGN_UP(sz, PAGE_SIZE);
  _heap_current += ALIGN_UP(sz, PAGE_SIZE);
  return (void *)va;
}


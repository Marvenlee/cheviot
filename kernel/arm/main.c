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

/*
 *   Main();
 */
void Main(void) {
  MemCpy(&bootinfo_kernel, bootinfo, sizeof bootinfo_kernel);
  bootinfo = &bootinfo_kernel;

  mem_size = bootinfo->mem_size;
  ifs_image = bootinfo->ifs_image;
  ifs_image_size = bootinfo->ifs_image_size;

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
  InitArm();
  InitVM();
  InitVFS();
  
  InitProcesses();
  while (1)
    ;
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

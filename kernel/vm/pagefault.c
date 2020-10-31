/*
 * Copyright 2014  Marven Gilhespie
 *
 * Licensed under the Apache License, segment_id 2.0 (the "License");
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
 * Page fault handling.
 */

#define KDEBUG

#include <kernel/arch.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>
#include <kernel/lists.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>
#include <string.h>


/*
 *
 */
int PageFault(vm_addr addr, bits32_t access) {
  struct Process *current;
  uint32_t page_flags;
  vm_addr paddr;
  vm_addr src_kva;
  vm_addr dst_kva;
  struct Pageframe *pf;
  vm_addr real_addr;

  current = GetCurrentProcess();
 
  real_addr = addr;
  addr = ALIGN_DOWN(addr, PAGE_SIZE);
    
  if (PmapExtract(&current->as, addr, &paddr, &page_flags) != 0) {
    // Page is not present
    return -1;
  }

  if ((page_flags & MEM_MASK) == MEM_PHYS) {
    return -1;
  } else if ((page_flags & MEM_MASK) != MEM_ALLOC) {
    return -1;
  } else if (!(access & PROT_WRITE)) {
    // FIXME: Make prefetch protection PROT_EXEC ???
    // Add additional if-else test
    return -1;
  } else if ((page_flags & (PROT_WRITE | MAP_COW)) == PROT_WRITE) {
    return -1;
  } else if ((page_flags & (PROT_WRITE | MAP_COW)) != (PROT_WRITE | MAP_COW)) {
    return -1;
  }

  pf = PmapPaToPf(paddr);

  KASSERT(pf->physical_addr == paddr);

  if (pf->reference_cnt > 1) {
    pf->reference_cnt--;
    if (PmapRemove(&current->as, addr) != 0) {
      PmapFlushTLBs();
      return -1;
    }

    // Now new page frame

    if ((pf = AllocPageframe(PAGE_SIZE)) == NULL) {
      return -1;
    }

    src_kva = PmapPaToVa(paddr);
    dst_kva = PmapPaToVa(pf->physical_addr);

    MemCpy ((void *)dst_kva, (void *)src_kva, PAGE_SIZE);

    page_flags = (page_flags | PROT_WRITE) & ~MAP_COW;

    if (PmapEnter(&current->as, addr, pf->physical_addr, page_flags) != 0) {
      PmapFlushTLBs();
      FreePageframe(pf);
      return -1;
    }

    pf->reference_cnt++;
    PmapFlushTLBs();
  } else if (pf->reference_cnt == 1) {
    if (PmapRemove(&current->as, addr) != 0) {
      PmapFlushTLBs();
      return -1;
    }

    page_flags = (page_flags | PROT_WRITE) & ~MAP_COW;

    if (PmapEnter(&current->as, addr, paddr, page_flags) != 0) {
      PmapFlushTLBs();
      pf->reference_cnt--;
      
      // TODO: Free page
      return -1;
    }

    PmapFlushTLBs();
  } else {
    KernelPanic();
  }

  return 0;
}


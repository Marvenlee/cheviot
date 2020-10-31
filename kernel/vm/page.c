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
 * Functions for allocating physical pages of system RAM.
 */

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
 * Allocate a 4k, 16k or 64k page, splitting larger slabs into smaller sizes if
 * needed. Could eventually do some page movement to make contiguous memory.
 */
struct Pageframe *AllocPageframe(vm_size size) {
  struct Pageframe *head = NULL;
  int t;

  if (size == 4096) {
    head = LIST_HEAD(&free_4k_pf_list);

    if (head != NULL) {
      LIST_REM_HEAD(&free_4k_pf_list, link);
    }
  }

  if (head == NULL && size == 16384) {
    head = LIST_HEAD(&free_16k_pf_list);

    if (head != NULL) {
      LIST_REM_HEAD(&free_16k_pf_list, link);
    }
  }

  if (head == NULL && size <= 65536) {
    head = LIST_HEAD(&free_64k_pf_list);

    if (head != NULL) {
      LIST_REM_HEAD(&free_64k_pf_list, link);
    }
  }

  if (head == NULL) {
    return NULL;
  }

  if (head->flags & PGF_INUSE) {
    Warn("**** Alloc Pageframe in-use");
  }

  // Split 64k slabs if needed into 4k or 16k allocations.

  if (head->size == 65536 && size == 16384) {
    for (t = 3; t > 0; t--) {
      head[(t * 16384) / PAGE_SIZE].size = 16384;
      head[(t * 16384) / PAGE_SIZE].flags = 0;
      LIST_ADD_HEAD(&free_16k_pf_list, head + t * (16384 / PAGE_SIZE), link);
    }
  } else if (head->size == 65536 && size == 4096) {
    for (t = 15; t > 0; t--) {
      head[(t * 4096) / PAGE_SIZE].size = 4096;
      head[(t * 4096) / PAGE_SIZE].flags = 0;
      LIST_ADD_HEAD(&free_4k_pf_list, head + t * (4096 / PAGE_SIZE), link);
    }
  }

  head->size = size;
  head->flags = PGF_INUSE;
  head->reference_cnt = 0;

  KASSERT(head->physical_addr < (128 * 1024 * 1024));

  PmapPageframeInit(&head->pmap_pageframe);

  vm_addr va = PmapPaToVa(head->physical_addr);

  // Ahh, this might be because root pages aren't all reserved?
  memset((void *)va, 0, size);

  return head;
}

/*
 *
 */
void FreePageframe(struct Pageframe *pf) {
  // FIXME: Debug FreePageframe, remove return
  return;
  
  
  KASSERT(pf != NULL);
  KASSERT((pf - pageframe_table) < max_pageframe);
  KASSERT(pf->size == 65536 || pf->size == 16384 || pf->size == 4096);

  KASSERT(pf->reference_cnt == 0);

  pf->flags = 0;

  if (pf->size == 65536) {
    LIST_ADD_TAIL(&free_64k_pf_list, pf, link);
  } else if (pf->size == 16384) {
    LIST_ADD_TAIL(&free_16k_pf_list, pf, link);
    CoalesceSlab(pf);
  } else {
    LIST_ADD_TAIL(&free_4k_pf_list, pf, link);
    CoalesceSlab(pf);
  }
}

/*
 *
 */
void CoalesceSlab(struct Pageframe *pf) {
  vm_addr base;
  vm_addr ceiling;
  vm_size stride;
  int t;

  KASSERT(pf != NULL);
  KASSERT((pf - pageframe_table) < max_pageframe);

  base = ALIGN_DOWN((pf - pageframe_table), (65536 / PAGE_SIZE));
  ceiling = base + 65536 / PAGE_SIZE;
  stride = pf->size / PAGE_SIZE;

  for (t = base; t < ceiling; t += stride) {
    if (pageframe_table[t].flags & PGF_INUSE) {
      return;
    }
  }

  for (t = base; t < ceiling; t += stride) {
    if (pf->size == 16384) {
      LIST_REM_ENTRY(&free_16k_pf_list, &pageframe_table[t], link);
    } else if (pf->size == 4096) {
      LIST_REM_ENTRY(&free_4k_pf_list, &pageframe_table[t], link);
    } else {
      KernelPanic();
    }
  }

  pageframe_table[base].flags = 0;
  pageframe_table[base].size = 65536;
  LIST_ADD_TAIL(&free_64k_pf_list, &pageframe_table[base], link)
}


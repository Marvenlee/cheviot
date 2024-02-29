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

//#define KDEBUG  1

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


/* @brief   Allocate a page in kernel memory
 *
 * @return  Virtual address of an allocated, page-sized area of memory
 *
 * TODO: Rename to kmalloc_slab(slab_manager_t *slab_manager, size_t size)
 * slabs can be 4k, 16k or 64k.  Can belong to a slab manager group of pages
 * or global.
 */ 
void *kmalloc_page(void)
{
  void *vaddr;
  struct Pageframe *pf;
  
  pf = alloc_pageframe(PAGE_SIZE);
  
  if (pf == NULL) {
    return NULL;
  }
  
  vaddr = (void *)pmap_pf_to_va(pf);
  return vaddr;
}


/*
 *
 */
void kfree_page(void *vaddr)
{
  struct Pageframe *pf;
  
  Info("kfree_page(%08x)", (uint32_t)vaddr);
  
  pf = pmap_va_to_pf((vm_addr)vaddr);
  
  if (pf != NULL) {
    free_pageframe(pf);
  }
}


/* @brief   Allocate a 4k, 16k or 64k page and return a Pageframe struct
 *
 * Splitting larger slabs into smaller sizes if needed.
 */
struct Pageframe *alloc_pageframe(vm_size size)
{
  struct Pageframe *head = NULL;
  int t;

  if (size == 4096) {
    head = LIST_HEAD(&free_4k_pf_list);

    if (head != NULL) {
      LIST_REM_HEAD(&free_4k_pf_list, link);
    }
  } else if (size == 16384) {
    head = LIST_HEAD(&free_16k_pf_list);

    if (head != NULL) {
      LIST_REM_HEAD(&free_16k_pf_list, link);
    }
  }

  if (head == NULL || size == 65536) {
    head = LIST_HEAD(&free_64k_pf_list);

    if (head != NULL) {
      LIST_REM_HEAD(&free_64k_pf_list, link);
    }
  }

  if (head == NULL) {
    Warn("no pageframe available");
    return NULL;
  }

  KASSERT ((head->flags & PGF_INUSE) == 0);

  // Split 64k slabs if needed into 4k or 16k allocations.
  if (head->size == 65536 && size == 16384) {
    for (t = 0; t < 4; t++) {
    
      head[t*4].size = 16384;
      head[t*4].flags = 0;
      
      if (t>0) {
        LIST_ADD_HEAD(&free_16k_pf_list, &head[t*4], link);
      }
    }
    
  } else if (head->size == 65536 && size == 4096) {
    for (t = 0; t < 16; t++) {
      head[t].size = 4096;
      head[t].flags = 0;

      if (t>0) {
        LIST_ADD_HEAD(&free_4k_pf_list, &head[t], link);
      }
    }
  }
  
  head->flags = PGF_INUSE;
  head->reference_cnt = 0;

  pmap_pageframe_init(&head->pmap_pageframe);

  vm_addr va = pmap_pa_to_va(head->physical_addr);

  // TODO: Add flag to clear page
  memset((void *)va, 0, size);

  return head;
}


/*
 *
 */

int dup_pageframe(struct Pageframe *pf)
{
  pf->reference_cnt++;
  return 0;
}


/*
 * FIXME: Debug FreePageframe, Finish VirtualFree
 */
void free_pageframe(struct Pageframe *pf)
{
  KASSERT(pf != NULL);
  KASSERT((pf - pageframe_table) < max_pageframe);
  KASSERT(pf->size == 65536 || pf->size == 16384 || pf->size == 4096);

  pf->reference_cnt--;

  if (pf->reference_cnt > 0) {
    return;
  }

  pf->flags = 0;

  if (pf->size == 65536) {
    LIST_ADD_TAIL(&free_64k_pf_list, pf, link);
  } else if (pf->size == 16384) {
    LIST_ADD_TAIL(&free_16k_pf_list, pf, link);
    coalesce_slab(pf);
  } else {
    LIST_ADD_TAIL(&free_4k_pf_list, pf, link);
    coalesce_slab(pf);
  }
}

/*
 * Coalesce free pages in a 64k block.
 *
 * The page allocator manages memory in three sizes of 4k, 16k and 64k pages.
 * If a 4k or 16k page is freed, check the other pages in the 64k aligned span
 * are also free. If all pages in a 64k span are free then coalesce into a
 * single 64k page. 
 */
void coalesce_slab(struct Pageframe *pf)
{
  vm_addr base;
  vm_addr ceiling;
  vm_size stride;
  int t;

#if 1   // FIXME: coalesce_slab
  Info("coalesce_slab");
  return;
#endif  

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
      Error("unknown page size coalescing");
      KernelPanic();
    }
  }

  pageframe_table[base].flags = 0;
  pageframe_table[base].size = 65536;
  LIST_ADD_TAIL(&free_64k_pf_list, &pageframe_table[base], link)
}


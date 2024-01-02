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
 * System calls for virtual memory management.
 */

//#define KDEBUG

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


/* @brief   Convert a user-mode virtual address to the page's physical address
 *
 * @param   addr, virtual address to convert
 * @return  physical address of the page
 *
 * To be used by drivers using DMA.
 */
vm_addr sys_virtualtophysaddr(vm_addr addr)
{
  struct Process *current;
  struct AddressSpace *as;
  vm_addr va;
  vm_addr pa;
  uint32_t flags;
 
  current = get_current_process();
  as = &current->as;
  va = ALIGN_DOWN(addr, PAGE_SIZE);
 
  // TODO: Check if current process has I/O privileges 
 
  if (pmap_is_page_present(as, va) == false) {
    return (vm_addr)NULL;
  }

  if (pmap_extract(as, va, &pa, &flags) != 0) {
    return (vm_addr)NULL;
  }

  return pa;
}


/* @brief   Allocate and map an area of memory
 *
 * @param   _addr,
 * @param   len,
 * @param   flags,
 * @return  virtual address of region or NULL on failure
 */
void *sys_virtualalloc(void *_addr, size_t len, bits32_t flags)
{
  struct Process *current;
  struct AddressSpace *as;
  vm_addr addr;
  vm_addr va;
  vm_addr paddr;
  vm_addr ceiling;
  struct Pageframe *pf;

  current = get_current_process();
  as = &current->as;
  addr = ALIGN_DOWN((vm_addr)_addr, PAGE_SIZE);
  len = ALIGN_UP(len, PAGE_SIZE);
  flags = (flags & ~VM_SYSTEM_MASK) | MEM_ALLOC;
  
  addr = segment_create(as, addr, len, SEG_TYPE_ALLOC, flags);

  if (addr == (vm_addr)NULL) {
    return NULL;
  }

  for (va = addr; va < addr + len; va += PAGE_SIZE) {
    if ((pf = alloc_pageframe(PAGE_SIZE)) == NULL) {
      goto cleanup;
    }

    if (pmap_enter(as, va, pf->physical_addr, flags) != 0) {
      goto cleanup;
    }
    
    pf->reference_cnt = 1;
  }

  pmap_flush_tlbs();
  return (void *)addr;

cleanup:
  ceiling = va;
  for (va = addr; va < ceiling; va += PAGE_SIZE) {
    if (pmap_extract(as, va, &paddr, NULL) == 0) {
      pf = pmap_pa_to_pf(paddr);
      free_pageframe(pf);
      pmap_remove(as, va);
    }
  }

  pmap_flush_tlbs();
  segment_free(as, addr, len);
  return NULL;
}


/* @brief   Map an area of physically contiguous memory
 *
 * Maps an area of physical memory such as IO device or framebuffer into the
 * address space of the calling process.
 */
void *sys_virtualallocphys(void *_addr, size_t len, bits32_t flags,
                         void *_paddr)
{
  struct Process *current;
  struct AddressSpace *as;
  vm_addr addr;
  vm_addr paddr;
  vm_addr va;
  vm_addr pa;
  vm_addr ceiling;

  current = get_current_process();
  as = &current->as;
  addr = ALIGN_DOWN((vm_addr)_addr, PAGE_SIZE);
  paddr = ALIGN_DOWN((vm_addr)_paddr, PAGE_SIZE);
  len = ALIGN_UP(len, PAGE_SIZE);
  flags = (flags & ~VM_SYSTEM_MASK) | MEM_PHYS;
  
  /* Replace with superuser uid 0 and gid 0 and gid 1.
    if (IsIOAllowed())

        if (!(current->flags & PROCF_ALLOW_IO))
    {
        return 0;
    }
*/
  addr = segment_create(as, addr, len, SEG_TYPE_PHYS, flags);

  if (addr == (vm_addr)NULL) {
    Warn("VirtualAllocPhys failed, no segment");
    return NULL;
  }

  for (va = addr, pa = paddr; va < addr + len; va += PAGE_SIZE, pa += PAGE_SIZE) {
    Info("phys mapping va:%08x, pa:%08x, flags:%08x", va, pa, flags);
    if (pmap_enter(as, va, pa, flags) != 0) {
      Warn("pmap_enter in VirtualAllocPhys failed");
      goto cleanup;
    }
  }

  pmap_flush_tlbs();
  return (void *)addr;

cleanup:
  ceiling = va;
  for (va = addr; va < ceiling; va += PAGE_SIZE) {
    if (pmap_extract(as, va, &paddr, NULL) == 0) {
      pmap_remove(as, va);
    }
  }
  
  pmap_flush_tlbs();
  segment_free(as, addr, len);
  return NULL;
}


/* @brief   Free an area of memory belonging to a process
 *
 * @param   _addr, start address of region to free
 * @param   sz, size of region to free
 * @return  0 on success, negative errno on failure
 */
int sys_virtualfree(void *_addr, size_t len)
{
  struct Process *current;
  struct AddressSpace *as;
  vm_addr addr;
  vm_addr va;

  current = get_current_process();
  as = &current->as;
  addr = ALIGN_DOWN((vm_addr)_addr, PAGE_SIZE);
  len = ALIGN_UP(len, PAGE_SIZE);

  for (va = addr; va < addr + len; va += PAGE_SIZE) {
    // FIXME: Get pageframe (pmap_extract?)  decrement ref count.  Free page if ref_cnt == 0
    // Finish FreePageframe
    
    pmap_remove(as, va);
  }

  pmap_flush_tlbs();  
  segment_free(as, addr, len);
  return 0;
}


/* @brief   Change the protection attributes of a region of the address space
 * 
 * Changes the read/write/execute protection attributes of a range of pages in
 * the current address space
 */
int sys_virtualprotect(void *_addr, size_t len, bits32_t flags)
{
  struct Process *current;
  struct AddressSpace *as;
  vm_addr addr;
  vm_addr va;
  vm_addr pa;

  current = get_current_process();
  as = &current->as;
  addr = ALIGN_DOWN((vm_addr)_addr, PAGE_SIZE);
  len = ALIGN_UP(len, PAGE_SIZE);

  for (va = addr; va < addr + len; va += PAGE_SIZE) {
    if (pmap_is_page_present(as, va) == false) {
      continue;
    }

    if (pmap_extract(as, va, &pa, &flags) != 0) {
      break;
    }

    if ((flags & MEM_PHYS) != MEM_PHYS && (flags & PROT_WRITE)) {
      // Read-Write mapping
      flags |= MAP_COW;
      if (pmap_protect(as, va, flags) != 0) {
        break;
      }
    } else if ((flags & MEM_PHYS) != MEM_PHYS) {
      // Read-only mapping
      if (pmap_protect(as, va, flags) != 0) {
        break;
      }
    } else {
      // Physical Mapping
      if (pmap_protect(as, va, flags) != 0) {
        break;
      }
    }
  }

  pmap_flush_tlbs();
  return 0;
}


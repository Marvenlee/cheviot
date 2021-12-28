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



SYSCALL vm_addr SysVirtualToPhysAddr(vm_addr addr) {
  struct Process *current;
  struct AddressSpace *as;
  vm_addr va;
  vm_addr pa;
  uint32_t flags;
 
  current = GetCurrentProcess();
  as = &current->as;
  va = ALIGN_DOWN(addr, PAGE_SIZE);
 
  if (PmapIsPagePresent(as, va) == FALSE) {
    return (vm_addr)NULL;
  }

  if (PmapExtract(as, va, &pa, &flags) != 0) {
    return (vm_addr)NULL;
  }

  return pa;
}


/*
 * Map an area of memory
 */
SYSCALL void *SysVirtualAlloc(void *_addr, size_t len, bits32_t flags) {
  struct Process *current;
  struct AddressSpace *as;
  vm_addr addr;
  vm_addr va;
  vm_addr paddr;
  vm_addr ceiling;
  struct Pageframe *pf;

  current = GetCurrentProcess();
  as = &current->as;
  addr = ALIGN_DOWN((vm_addr)_addr, PAGE_SIZE);
  len = ALIGN_UP(len, PAGE_SIZE);
  flags = (flags & ~VM_SYSTEM_MASK) | MEM_ALLOC;


  Info("VirtualAlloc (a:%08x, s:%08x, f:%08x", addr, len, flags);

  addr = SegmentCreate(as, addr, len, SEG_TYPE_ALLOC, flags);

  if (addr == (vm_addr)NULL) {
    return NULL;
  }

  for (va = addr; va < addr + len; va += PAGE_SIZE) {
    if ((pf = AllocPageframe(PAGE_SIZE)) == NULL) {
      goto cleanup;
    }

    if (PmapEnter(as, va, pf->physical_addr, flags) != 0) {
      goto cleanup;
    }
    
    pf->reference_cnt = 1;
  }

  PmapFlushTLBs();
  return (void *)addr;

cleanup:
  ceiling = va;
  for (va = addr; va < ceiling; va += PAGE_SIZE) {
    if (PmapExtract(as, va, &paddr, NULL) == 0) {
      pf = PmapPaToPf(paddr);
      FreePageframe(pf);
      PmapRemove(as, va);
    }
  }

  PmapFlushTLBs();
  SegmentFree(as, addr, len);
  return NULL;
}

/*
 * Maps an area of physical memory such as IO device or framebuffer into the
 * address space of the calling process.
 */

SYSCALL void *SysVirtualAllocPhys(void *_addr, size_t len, bits32_t flags,
                         void *_paddr) {
  struct Process *current;
  struct AddressSpace *as;
  vm_addr addr;
  vm_addr paddr;
  vm_addr va;
  vm_addr pa;
  vm_addr ceiling;

  current = GetCurrentProcess();
  as = &current->as;
  addr = ALIGN_DOWN((vm_addr)_addr, PAGE_SIZE);
  paddr = ALIGN_DOWN((vm_addr)_paddr, PAGE_SIZE);
  len = ALIGN_UP(len, PAGE_SIZE);
  flags = (flags & ~VM_SYSTEM_MASK) | MEM_PHYS;

  
  Info("VirtualAllocPhys (a:%08x, s:%08x, pa:%08x, f:%08x", addr, len, paddr, flags);

  /* Replace with superuser uid 0 and gid 0 and gid 1.
    if (IsIOAllowed())

        if (!(current->flags & PROCF_ALLOW_IO))
    {
        return 0;
    }
*/
  addr = SegmentCreate(as, addr, len, SEG_TYPE_PHYS, flags);

  if (addr == (vm_addr)NULL) {
    Warn("VirtualAllocPhys failed, no segment");
    return NULL;
  }

  for (va = addr, pa = paddr; va < addr + len; va += PAGE_SIZE, pa += PAGE_SIZE) {
    if (PmapEnter(as, va, pa, flags) != 0) {
      Warn("PmapEnter in VirtualAllocPhys failed");
      goto cleanup;
    }
  }

  PmapFlushTLBs();
  return (void *)addr;

cleanup:
  ceiling = va;
  for (va = addr; va < ceiling; va += PAGE_SIZE) {
    if (PmapExtract(as, va, &paddr, NULL) == 0) {
      PmapRemove(as, va);
    }
  }
  
  PmapFlushTLBs();
  SegmentFree(as, addr, len);
  return NULL;
}

/*
 *
 */
SYSCALL int SysVirtualFree(void *_addr, size_t len) {
  struct Process *current;
  struct AddressSpace *as;
  vm_addr addr;
  vm_addr va;

  current = GetCurrentProcess();
  as = &current->as;
  addr = ALIGN_DOWN((vm_addr)_addr, PAGE_SIZE);
  len = ALIGN_UP(len, PAGE_SIZE);

  Info("VirtualFree addr:%08x, sz=%08x", addr, len);

  for (va = addr; va < addr + len; va += PAGE_SIZE) {
    // TODO: Get pageframe (pmapextract?)  decrement ref count.  Free page 
        
    PmapRemove(as, va);
  }

  PmapFlushTLBs();  
  SegmentFree(as, addr, len);
  return 0;
}

/*
 * Change the read/write/execute protection attributes of a range of pages in
 * the current address space
 */
SYSCALL int SysVirtualProtect(void *_addr, size_t len, bits32_t flags) {
  struct Process *current;
  struct AddressSpace *as;
  vm_addr addr;
  vm_addr va;
  vm_addr pa;

  current = GetCurrentProcess();
  as = &current->as;
  addr = ALIGN_DOWN((vm_addr)_addr, PAGE_SIZE);
  len = ALIGN_UP(len, PAGE_SIZE);

  Info("VirtualProtect (a:%08x, s:%08x, f:%08x", addr, len, flags);

  for (va = addr; va < addr + len; va += PAGE_SIZE) {
    if (PmapIsPagePresent(as, va) == FALSE) {
      continue;
    }

    if (PmapExtract(as, va, &pa, &flags) != 0) {
      break;
    }

    if ((flags & MEM_PHYS) != MEM_PHYS && (flags & PROT_WRITE)) {
      // Read-Write mapping
      flags |= MAP_COW;
      if (PmapProtect(as, va, flags) != 0) {
        break;
      }
    } else if ((flags & MEM_PHYS) != MEM_PHYS) {
      // Read-only mapping
      if (PmapProtect(as, va, flags) != 0) {
        break;
      }
    } else {
      // Physical Mapping
      if (PmapProtect(as, va, flags) != 0) {
        break;
      }
    }
  }

  PmapFlushTLBs();
  return 0;
}


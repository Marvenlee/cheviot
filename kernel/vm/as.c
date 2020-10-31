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

//#define KDEBUG 1

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
int ForkAddressSpace(struct AddressSpace *new_as, struct AddressSpace *old_as) {
  vm_addr vpt, va, pa;
  bits32_t flags;
  struct Pageframe *pf;

  KASSERT(new_as != NULL);
  KASSERT(old_as != NULL);

  if (PmapCreate(new_as) != 0) {
    return -1;
  }

  new_as->segment_cnt = old_as->segment_cnt;
  for (int t = 0; t <= new_as->segment_cnt; t++) {
    new_as->segment_table[t] = old_as->segment_table[t];
  }

  for (vpt = VM_USER_BASE; vpt < VM_USER_CEILING;
       vpt += PAGE_SIZE * N_PAGETABLE_PTE) {
    if (PmapIsPageTablePresent(old_as, vpt) == FALSE) {
      continue;
    }

    for (va = vpt; va < vpt + PAGE_SIZE * N_PAGETABLE_PTE; va += PAGE_SIZE) {
      if (PmapIsPagePresent(old_as, va) == FALSE) {
        continue;
      }

      if (PmapExtract(old_as, va, &pa, &flags) != 0) {
        goto cleanup;
      }
      
     
      if ((flags & MEM_PHYS) != MEM_PHYS && (flags & PROT_WRITE)) {
        // Read-Write mapping, Mark page in both as COW and read-only;
        flags |= MAP_COW;
     
        if (PmapProtect(old_as, va, flags) != 0) {
          goto cleanup;
        }

        if (PmapEnter(new_as, va, pa, flags) != 0) {
          goto cleanup;
        }

        pf = PmapPaToPf(pa);
        pf->reference_cnt++;
      } else if ((flags & MEM_PHYS) != MEM_PHYS) {
        // TODO: Should flags also map it as COW?
        // Read-only mapping

        if (PmapEnter(new_as, va, pa, flags) != 0) {
          goto cleanup;
        }

        pf = PmapPaToPf(pa);
        pf->reference_cnt++;
      } else {
        // Physical Mapping
        if (PmapEnter(new_as, va, pa, flags) != 0) {
          goto cleanup;
        }
      }
    }
  }

  PmapFlushTLBs();
  return 0;

cleanup:
  Info("ForkaddressSpace failed");

  CleanupAddressSpace(new_as);
  FreeAddressSpace(new_as);
  return -1;
}

/*
 *
 */
void CleanupAddressSpace(struct AddressSpace *as) {
  struct Pageframe *pf;
  vm_addr pa;
  vm_addr vpt;
  vm_addr va;
  uint32_t flags;

  for (vpt = VM_USER_BASE; vpt <= VM_USER_CEILING;
       vpt += PAGE_SIZE * N_PAGETABLE_PTE) {
    if (PmapIsPageTablePresent(as, vpt) == FALSE) {
      continue;
    }

    for (va = vpt; va < vpt + PAGE_SIZE * N_PAGETABLE_PTE; va += PAGE_SIZE) {
      if (PmapIsPagePresent(as, va) == FALSE) {
        continue;
      }

      if (PmapExtract(as, va, &pa, &flags) != 0) {
        continue;
      }

      if (PmapRemove(as, va) != 0) {
        continue;
      }

      if ((flags & MEM_PHYS) == MEM_PHYS) {
        continue;
      }

      pf = PmapPaToPf(pa);
      pf->reference_cnt--;

      if (pf->reference_cnt == 0) {
        FreePageframe(pf);
      }
    }
  }

  as->segment_cnt = 1;
  as->segment_table[0] = VM_USER_BASE | SEG_TYPE_FREE;
  as->segment_table[1] = VM_USER_CEILING | SEG_TYPE_CEILING;

  PmapFlushTLBs();
}

/*
 * Frees CPU dependent virtual memory management structures.
 */
void FreeAddressSpace(struct AddressSpace *as) {
  PmapDestroy(as);
  PmapFlushTLBs();
}


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
 * ARM-Specific memory management code.  Deals with converting high-level
 * VirtSeg and PhysSeg segment tables into CPU page tables.
 *
 * The CPU's page directories are 16k and the page tables are 1k.  We use 4K
 * page size and so are able to fit 1K of page tables plus a further 12 bytes
 * per page table entry for flags and linked list next/prev pointers to keep
 * track of page table entries referencing a Pageframe.
 */

#define KDEBUG

#include <kernel/arm/arm.h>
#include <kernel/arm/globals.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>

uint32_t *PmapAllocPageTable(void);
void PmapFreePageTable(uint32_t *pt);

uint32_t *PmapAllocPageDirectory(void);
void PmapFreePageDirectory(uint32_t *pd);

int PmapPageTableIncRefCnt(uint32_t *pt);
int PmapPageTableDecRefCnt(uint32_t *pt);

static uint32_t PmapCalcPABits(bits32_t flags) {
  uint32_t pa_bits;

  pa_bits = L2_TYPE_S;

  if ((flags & PROT_WRITE) && !(flags & MAP_COW)) {
    pa_bits |= L2_AP(AP_U | AP_W);
  } else {
    pa_bits |= L2_AP(AP_U); // | L2_APX;
  }

  if ((flags & PROT_EXEC) != PROT_EXEC) {
    // FIXME: Support PROT_EXEC
    //        pa_bits |= L2_NX;
  }

  if ((flags & MEM_MASK) == MEM_ALLOC) {
    if ((flags & CACHE_MASK) == CACHE_DEFAULT)
      pa_bits |= L2_B | L2_C;
    else if ((flags & CACHE_MASK) == CACHE_WRITEBACK)
      pa_bits |= L2_B | L2_C;
    else if ((flags & CACHE_MASK) == CACHE_WRITETHRU)     // can IO be write-thru?
      pa_bits |= L2_C;
    else if ((flags & CACHE_MASK) == CACHE_WRITECOMBINE)
      pa_bits |= L2_B;
    else if ((flags & CACHE_MASK) == CACHE_UNCACHEABLE)
      pa_bits |= 0;
    else
      pa_bits |= L2_B | L2_C;
  }

  if ((flags & MEM_MASK) == MEM_PHYS) {
    if ((flags & CACHE_MASK) == CACHE_DEFAULT)
      pa_bits |= 0;
    else if ((flags & CACHE_MASK) == CACHE_WRITEBACK)
      pa_bits |= L2_B | L2_C;
    else if ((flags & CACHE_MASK) == CACHE_WRITETHRU)
      pa_bits |= L2_C;
    else if ((flags & CACHE_MASK) == CACHE_WRITECOMBINE)
      pa_bits |= L2_B;
    else if ((flags & CACHE_MASK) == CACHE_UNCACHEABLE)
      pa_bits |= 0;
    else
      pa_bits |= 0;
  }

  return pa_bits;
}

/*
 * 4k page tables,  1k real PTEs,  3k virtual-page linked list and flags (packed
 * 12 bytes)
 */
int PmapEnter(struct AddressSpace *as, vm_addr addr, vm_addr paddr,
              bits32_t flags) {
  struct Pmap *pmap;
  uint32_t *pt, *phys_pt;
  int pde_idx, pte_idx;
  uint32_t pa_bits;
  struct Pageframe *pf;
  struct Pageframe *ptpf;
  struct PmapVPTE *vpte;
  struct PmapVPTE *vpte_base;

  if (addr == 0) {
    return -EFAULT;
  }

  pa_bits = PmapCalcPABits(flags);
  pmap = &as->pmap;
  pde_idx = (addr & L1_ADDR_BITS) >> L1_IDX_SHIFT;

  if ((pmap->l1_table[pde_idx] & L1_TYPE_MASK) == L1_TYPE_INV) {
    if ((pt = PmapAllocPageTable()) == NULL) {
      return -ENOMEM;
    }

    phys_pt = (uint32_t *)PmapVaToPa((vm_addr)pt);
    pmap->l1_table[pde_idx] = (uint32_t)phys_pt | L1_TYPE_C;
  } else {
    phys_pt = (uint32_t *)(pmap->l1_table[pde_idx] & L1_C_ADDR_MASK);
    pt = (uint32_t *)PmapPaToVa((vm_addr)phys_pt);
  }

  pte_idx = (addr & L2_ADDR_BITS) >> L2_IDX_SHIFT;
  vpte_base = (struct PmapVPTE *)((uint8_t *)pt + VPTE_TABLE_OFFS);
  vpte = vpte_base + pte_idx;

  if ((pt[pte_idx] & L2_TYPE_MASK) != L2_TYPE_INV) {
    PmapFreePageTable(pt);
    pmap->l1_table[pde_idx] = L1_TYPE_INV;
    return -EINVAL;
  }

  if ((flags & MEM_MASK) != MEM_PHYS) {
    pf = &pageframe_table[paddr / PAGE_SIZE];
    vpte->flags = flags;

    LIST_ADD_HEAD(&pf->pmap_pageframe.vpte_list, vpte, link);
    pt[pte_idx] = paddr | pa_bits;
  } else {
    vpte->flags = flags;
    pt[pte_idx] = paddr | pa_bits;
  }

  ptpf = PmapVaToPf((vm_addr)pt);
  ptpf->reference_cnt++;

  return 0;
}

/*
 * Unmaps a segment from the address space pointed to by pmap.
 */
int PmapRemove(struct AddressSpace *as, vm_addr va) {
  struct Pmap *pmap;
  uint32_t *pt, *phys_pt;
  int pde_idx, pte_idx;
  vm_addr current_paddr;
  struct Pageframe *pf;
  struct Pageframe *ptpf;
  struct PmapVPTE *vpte;
  struct PmapVPTE *vpte_base;

  // TODO : Check user base user ceiling
  if (va == 0) {
    return -EFAULT;
  }

  pmap = &as->pmap;
  pde_idx = (va & L1_ADDR_BITS) >> L1_IDX_SHIFT;

  if ((pmap->l1_table[pde_idx] & L1_TYPE_MASK) == L1_TYPE_INV) {
    return -EINVAL;
  }

  phys_pt = (uint32_t *)(pmap->l1_table[pde_idx] & L1_C_ADDR_MASK);
  pt = (uint32_t *)PmapPaToVa((vm_addr)phys_pt);

  pte_idx = (va & L2_ADDR_BITS) >> L2_IDX_SHIFT;
  current_paddr = pt[pte_idx] & L2_ADDR_MASK;

  vpte_base = (struct PmapVPTE *)((uint8_t *)pt + VPTE_TABLE_OFFS);
  vpte = vpte_base + pte_idx;

  // Unmap any existing mapping, then map either anon or phys mem
  if ((pt[pte_idx] & L2_TYPE_MASK) == L2_TYPE_INV) {
    return -EINVAL;
  }

  if ((vpte->flags & VPTE_PHYS) == 0) {
    pf = PmapPaToPf(current_paddr);
    LIST_REM_ENTRY(&pf->pmap_pageframe.vpte_list, vpte, link);
  }

  vpte->flags = 0;
  pt[pte_idx] = L2_TYPE_INV;

  ptpf = PmapVaToPf((vm_addr)pt);
  ptpf->reference_cnt--;

  if (ptpf->reference_cnt == 0) {
    PmapFreePageTable(pt);
    pmap->l1_table[pde_idx] = L1_TYPE_INV;
  }

  return 0;
}

/*
 * Change protections on a page
 */
int PmapProtect(struct AddressSpace *as, vm_addr addr, bits32_t flags) {
  struct Pmap *pmap;
  uint32_t *pt, *phys_pt;
  int pde_idx, pte_idx;
  struct PmapVPTE *vpte;
  struct PmapVPTE *vpte_base;
  uint32_t pa_bits;
  vm_addr pa;

  pmap = &as->pmap;

  if (addr == 0) {
    return 0;
  }

  pde_idx = (addr & L1_ADDR_BITS) >> L1_IDX_SHIFT;

  if ((pmap->l1_table[pde_idx] & L1_TYPE_MASK) == L1_TYPE_INV) {
    return -1;
  } else {
    phys_pt = (uint32_t *)(pmap->l1_table[pde_idx] & L1_C_ADDR_MASK);
    pt = (uint32_t *)PmapPaToVa((vm_addr)phys_pt);
  }

  pte_idx = (addr & L2_ADDR_BITS) >> L2_IDX_SHIFT;
  vpte_base = (struct PmapVPTE *)((uint8_t *)pt + VPTE_TABLE_OFFS);
  vpte = vpte_base + pte_idx;
  pa = pt[pte_idx] & L2_ADDR_MASK;

  if ((pt[pte_idx] & L2_TYPE_MASK) == L2_TYPE_INV) {
    return -1;
  }

  vpte->flags = flags;
  pa_bits = PmapCalcPABits(vpte->flags);
  pt[pte_idx] = pa | pa_bits;
  return 0;
}

/*
 * Extract the physical address and flags associates with a virtual address
 */
int PmapExtract(struct AddressSpace *as, vm_addr va, vm_addr *pa,
                uint32_t *flags) {
  struct Pmap *pmap;
  uint32_t *pt, *phys_pt;
  int pde_idx, pte_idx;
  vm_addr current_paddr;
  struct PmapVPTE *vpte;
  struct PmapVPTE *vpte_base;

  pmap = &as->pmap;
  pde_idx = (va & L1_ADDR_BITS) >> L1_IDX_SHIFT;

  if ((pmap->l1_table[pde_idx] & L1_TYPE_MASK) == L1_TYPE_INV) {
    return -1;
  } else {
    phys_pt = (uint32_t *)(pmap->l1_table[pde_idx] & L1_C_ADDR_MASK);
    pt = (uint32_t *)PmapPaToVa((vm_addr)phys_pt);
  }

  pte_idx = (va & L2_ADDR_BITS) >> L2_IDX_SHIFT;
  vpte_base = (struct PmapVPTE *)((uint8_t *)pt + VPTE_TABLE_OFFS);
  vpte = vpte_base + pte_idx;
  current_paddr = pt[pte_idx] & L2_ADDR_MASK;

  if ((pt[pte_idx] & L2_TYPE_MASK) == L2_TYPE_INV) {
    return -1;
  }

  *pa = current_paddr;
  *flags = vpte->flags;
  return 0;
}

/*
 *
 */
bool PmapIsPageTablePresent(struct AddressSpace *as, vm_addr addr) {
  struct Pmap *pmap;
  int pde_idx;

  pmap = &as->pmap;
  pde_idx = (addr & L1_ADDR_BITS) >> L1_IDX_SHIFT;

  if ((pmap->l1_table[pde_idx] & L1_TYPE_MASK) == L1_TYPE_INV) {
    return false;
  } else {
    return true;
  }
}

/*
 *
 */
bool PmapIsPagePresent(struct AddressSpace *as, vm_addr addr) {
  struct Pmap *pmap;
  uint32_t *pt, *phys_pt;
  int pde_idx, pte_idx;

  pmap = &as->pmap;
  pde_idx = (addr & L1_ADDR_BITS) >> L1_IDX_SHIFT;

  if ((pmap->l1_table[pde_idx] & L1_TYPE_MASK) == L1_TYPE_INV) {
    return false;
  } else {
    phys_pt = (uint32_t *)(pmap->l1_table[pde_idx] & L1_C_ADDR_MASK);
    pt = (uint32_t *)PmapPaToVa((vm_addr)phys_pt);

    pte_idx = (addr & L2_ADDR_BITS) >> L2_IDX_SHIFT;

    if ((pt[pte_idx] & L2_TYPE_MASK) != L2_TYPE_INV)
      return true;
    else
      return false;
  }
}

/*
 *
 */
uint32_t *PmapAllocPageTable(void) {
  int t;
  uint32_t *pt;
  struct Pageframe *pf;

  if ((pf = AllocPageframe(VPAGETABLE_SZ)) == NULL)
    return NULL;

  pt = (uint32_t *)PmapPfToVa(pf);

  for (t = 0; t < 256; t++) {
    *(pt + t) = L2_TYPE_INV;
  }

  return pt;
}

/*
 *
 */
void PmapPageframeInit(struct PmapPageframe *ppf) {
  LIST_INIT(&ppf->vpte_list);
}

/*
 *
 */
void PmapFreePageTable(uint32_t *pt) {
  struct Pageframe *pf;
  pf = PmapVaToPf((vm_addr)pt);

  FreePageframe(pf);
}

/*
 *
 */
int PmapCreate(struct AddressSpace *as) {
  uint32_t *pd;
  int t;
  struct Pageframe *pf;

  if ((pf = AllocPageframe(PAGEDIR_SZ)) == NULL) {
    return -1;
  }

  pd = (uint32_t *)PmapPfToVa(pf);

  for (t = 0; t < 2048; t++) {
    *(pd + t) = L1_TYPE_INV;
  }

  for (t = 2048; t < 4096; t++) {
    *(pd + t) = root_pagedir[t];
  }

  as->pmap.l1_table = pd;
  return 0;
}

/*
 *
 */
void PmapDestroy(struct AddressSpace *as) {
  uint32_t *pd;
  struct Pageframe *pf;

  pd = as->pmap.l1_table;
  pf = PmapVaToPf((vm_addr)pd);
  FreePageframe(pf);
}

/*
 * Checks if the flags argument of VirtualAlloc() and VirtualAllocPhys() is
 * valid and supported.  Returns 0 on success, -1 otherwise.
 */
int PmapSupportsCachePolicy(bits32_t flags) {
  return 0;
}

/*
 *
 */
vm_addr PmapPfToPa(struct Pageframe *pf) {
  return (vm_addr)pf->physical_addr;
}

/*
 *
 */
struct Pageframe *PmapPaToPf(vm_addr pa) {
  return &pageframe_table[(vm_addr)pa / PAGE_SIZE];
}

/*
 *
 */
vm_addr PmapPfToVa(struct Pageframe *pf) {
  return PmapPaToVa((pf->physical_addr));
}

/*
 *
 */
struct Pageframe *PmapVaToPf(vm_addr va) {
  return &pageframe_table[PmapVaToPa(va) / PAGE_SIZE];
}

/*
 *
 */
vm_addr PmapVaToPa(vm_addr vaddr) {
  return vaddr - 0x80000000;
}

/*
 *
 */
vm_addr PmapPaToVa(vm_addr paddr) {
  return paddr + 0x80000000;
}

/*
 * Flushes the CPU's TLBs once page tables are updated.
 */

void PmapFlushTLBs(void) {
  //    struct Process *current;
  // NEED an IPI invvalidate for multicore
  //    current = GetCurrentProcess();
  //    SetPageDirectory ((void *)(PmapVaToPa
  //    ((vm_addr)current->as.pmap.l1_table)));
  InvalidateTLB();
  FlushAllCaches();
}

/*
 * Switches the address space from the current process to the next process.
 * TODO: Need to update code, probably don't need to flush caches. 
 */

void PmapSwitch(struct Process *next, struct Process *current) {
  //    if (next != NULL && next != current) {
  SetPageDirectory((void *)(PmapVaToPa((vm_addr)next->as.pmap.l1_table)));
  InvalidateTLB();
  FlushAllCaches();
  //    }
}

/*
 * 4k page tables,  1k real PTEs,  3k virtual-page linked list and flags (packed
 * 12 bytes)
 */
int PmapCacheEnter(vm_addr addr, vm_addr paddr) {
  uint32_t *pt, *phys_pt;
  int pde_idx, pte_idx;
  uint32_t pa_bits;
  struct Pageframe *pf;
  struct PmapVPTE *vpte;
  struct PmapVPTE *vpte_base;

  pa_bits = L2_TYPE_S | L2_AP(AP_U);
  pa_bits |= L2_AP(AP_W);
  pa_bits |= L2_NX;
  pa_bits |= L2_B | L2_C;

  pde_idx = (addr & L1_ADDR_BITS) >> L1_IDX_SHIFT;

  phys_pt = (uint32_t *)(root_pagedir[pde_idx] & L1_C_ADDR_MASK);

  pt = (uint32_t *)PmapPaToVa((vm_addr)phys_pt);

  pte_idx = (addr & L2_ADDR_BITS) >> L2_IDX_SHIFT;

  vpte_base = (struct PmapVPTE *)((uint8_t *)pt + VPTE_TABLE_OFFS);
  vpte = vpte_base + pte_idx;
  pf = &pageframe_table[paddr / PAGE_SIZE];
  vpte->flags = PROT_READ | PROT_WRITE;
  LIST_ADD_HEAD(&pf->pmap_pageframe.vpte_list, vpte, link);
  // TODO: Increment/Decrement pf reference cnt or not?

  pt[pte_idx] = paddr | pa_bits;
  return 0;
}

/*
 * Unmaps a segment from the address space pointed to by pmap.
 */
int PmapCacheRemove(vm_addr va) {
  uint32_t *pt, *phys_pt;
  int pde_idx, pte_idx;
  vm_addr current_paddr;
  struct Pageframe *pf;
  struct PmapVPTE *vpte;
  struct PmapVPTE *vpte_base;

  pde_idx = (va & L1_ADDR_BITS) >> L1_IDX_SHIFT;

  phys_pt = (uint32_t *)(root_pagedir[pde_idx] & L1_C_ADDR_MASK);
  pt = (uint32_t *)PmapPaToVa((vm_addr)phys_pt);

  pte_idx = (va & L2_ADDR_BITS) >> L2_IDX_SHIFT;
  current_paddr = pt[pte_idx] & L2_ADDR_MASK;

  vpte_base = (struct PmapVPTE *)((uint8_t *)pt + VPTE_TABLE_OFFS);
  vpte = vpte_base + pte_idx;

  pf = PmapPaToPf(current_paddr);
  LIST_REM_ENTRY(&pf->pmap_pageframe.vpte_list, vpte, link);

  vpte->flags = 0;
  pt[pte_idx] = L2_TYPE_INV;

  // TODO: Increment/Decrement pf reference cnt or not?

  return 0;
}

/*
 * Get the physcial address of a page in the file cache
 */
int PmapCacheExtract(vm_addr va, vm_addr *pa) {
  uint32_t *pt, *phys_pt;
  int pde_idx, pte_idx;

  pde_idx = (va & L1_ADDR_BITS) >> L1_IDX_SHIFT;

  phys_pt = (uint32_t *)(root_pagedir[pde_idx] & L1_C_ADDR_MASK);
  pt = (uint32_t *)PmapPaToVa((vm_addr)phys_pt);

  pte_idx = (va & L2_ADDR_BITS) >> L2_IDX_SHIFT;
  *pa = pt[pte_idx] & L2_ADDR_MASK;
  return 0;
}

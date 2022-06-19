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
 * functions for managing spans of virtual memory.
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

/**
 *
 */

vm_addr segment_create(struct AddressSpace *as, vm_offset addr, vm_size size,
                      int type, bits32_t flags) {
  vm_addr *seg;
  vm_addr base;
  int t;

//  addr = ALIGN_DOWN(addr, 4096);
//  size = ALIGN_UP(size, 4096);

  if (size == 0) {
    return (vm_addr)NULL;
  }

  if (flags & MAP_FIXED) {
    if ((seg = segment_find(as, addr)) == NULL) {
      return (vm_addr)NULL;
    }

    if ((*seg & SEG_TYPE_MASK) != SEG_TYPE_FREE) {
      return (vm_addr)NULL;
    }

    if ((addr + size) > (*(seg + 1) & SEG_ADDR_MASK)) {
      return (vm_addr)NULL;
    }

  } else if ((seg = segment_alloc(as, size, flags, &addr)) == NULL) {
    return (vm_addr)NULL;
  }

  t = seg - as->segment_table;

  if ((*seg & SEG_ADDR_MASK) == addr &&
      (*(seg + 1) & SEG_ADDR_MASK) == addr + size) {
    /* Perfect fit, initialize region */
    *seg = (*seg & ~SEG_TYPE_MASK) | (type & SEG_TYPE_MASK);
  } else if ((*seg & SEG_ADDR_MASK) < addr &&
             (*(seg + 1) & SEG_ADDR_MASK) > addr + size) {
    /* In the middle, between two parts */

    base = *seg & SEG_ADDR_MASK;

    segment_insert(as, t, 2);

    *seg = base | SEG_TYPE_FREE;
    *(seg + 1) = addr | (type & SEG_TYPE_MASK);
    *(seg + 2) = (addr + size) | SEG_TYPE_FREE;
  } else if ((*seg & SEG_ADDR_MASK) == addr &&
             (*(seg + 1) & SEG_ADDR_MASK) > addr + size) {
    /* Starts at bottom of area */

    base = *seg & SEG_ADDR_MASK;

    segment_insert(as, t, 1);

    *seg = addr | (type & SEG_TYPE_MASK);
    *(seg + 1) = (addr + size) | SEG_TYPE_FREE;
  } else {
    /* Starts at top of area */

    base = *seg & SEG_ADDR_MASK;

    segment_insert(as, t, 1);

    *seg = base | SEG_TYPE_FREE;
    *(seg + 1) = addr | (type & SEG_TYPE_MASK);
  }

  return addr;
}

void segment_free(struct AddressSpace *as, vm_addr base, vm_size size) {
  int lo;
  int hi;
  
  if (base == (vm_addr)NULL) {
    return;
  }

  lo = segment_splice(as, base);
  hi = segment_splice(as, base + size);

  for (int t = lo; t < hi; t++) {
    as->segment_table[t] =
        (as->segment_table[t] & SEG_ADDR_MASK) | SEG_TYPE_FREE;
  }

  segment_coalesce(as);
}

/*
 * MemAreaInsert();
 *
 * Replace code with a memmove/memcpy() ?
 *
 * May want to optimize to avoid moving entries if 'cnt' contiguous areas are
 * free
 * starting at idx. Coalesce them if possible?
 *
 */

void segment_insert(struct AddressSpace *as, int index, int cnt) {
  int t;

  for (t = as->segment_cnt - 1 + cnt; t >= (index + cnt); t--) {
    as->segment_table[t] = as->segment_table[t - cnt];
  }

  as->segment_cnt += cnt;
  as->segment_table[as->segment_cnt] = VM_USER_CEILING | SEG_TYPE_CEILING;
}

/*
 *
 */

vm_addr *segment_find(struct AddressSpace *as, vm_addr addr) {
  int low = 0;
  int high = as->segment_cnt - 1;
  int mid;

  // FIXME:  How does this handle the SEG_TYPE_CEILING?

  while (low <= high) {
    mid = low + ((high - low) / 2);

    if (addr >= (as->segment_table[mid + 1] & SEG_ADDR_MASK))
      low = mid + 1;
    else if (addr < (as->segment_table[mid] & SEG_ADDR_MASK))
      high = mid - 1;
    else
      return &as->segment_table[mid];
  }

  return NULL;
}

/*
 * MemAreaCoalesce();
 */

void segment_coalesce(struct AddressSpace *as) {
  int t, s;

  for (t = 0, s = 0; t <= as->segment_cnt; t++) {
    if (s > 0 && (as->segment_table[s - 1] & SEG_TYPE_MASK) == SEG_TYPE_FREE &&
        (as->segment_table[t] & SEG_TYPE_MASK) == SEG_TYPE_FREE) {
      as->segment_table[s - 1] = as->segment_table[t];
    } else {
      as->segment_table[s] = as->segment_table[t];
      s++;
    }
  }

  as->segment_cnt = s;
  as->segment_table[as->segment_cnt] = VM_USER_CEILING | SEG_TYPE_CEILING;
}

/*
 * FIXME:  Allocate above *ret_addr
 */

vm_addr *segment_alloc(struct AddressSpace *as, vm_size size, uint32_t flags,
                      vm_addr *ret_addr) {
  int i;
  vm_addr addr;

  if ((flags & MAP_FIXED) == 0 && *ret_addr == 0) {
    addr = 0x00800000;
  } else {
    addr = *ret_addr;
  }

  for (i = 0; i < as->segment_cnt; i++) {
    if ((as->segment_table[i] & SEG_TYPE_MASK) != SEG_TYPE_FREE)
      continue;

    if (((as->segment_table[i] & SEG_ADDR_MASK) <= addr) &&
        ((addr + size) < (as->segment_table[i + 1] & SEG_ADDR_MASK))) {
      *ret_addr = addr;
      return &as->segment_table[i];
    }

    if (((as->segment_table[i] & SEG_ADDR_MASK) >= addr) &&
        size <= ((as->segment_table[i + 1] & SEG_ADDR_MASK) -
                 (as->segment_table[i] & SEG_ADDR_MASK))) {
      *ret_addr = (as->segment_table[i] & SEG_ADDR_MASK);
      return &as->segment_table[i];
    }
  }

  return NULL;
}

int segment_splice(struct AddressSpace *as, vm_addr addr) {
  // Find a segment and if necessary split it, do not change flags etc.
  vm_addr *seg;
  int i;

  if (addr == VM_USER_CEILING) {
    return NSEGMENT;
  }

  seg = segment_find(as, addr);

  if (seg == NULL) {
    return -1;
  }

  i = seg - as->segment_table;

  if (addr == (*seg & SEG_ADDR_MASK)) {
    return i;
  }

  segment_insert(as, i, 1);

  as->segment_table[i + 1] = as->segment_table[i];

  return i;
}


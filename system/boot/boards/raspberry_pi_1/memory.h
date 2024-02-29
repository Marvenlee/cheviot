/*
 * Copyright 2023  Marven Gilhespie
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

#ifndef BOARDS_RASPBERRY_PI_1_MEMORY_H
#define BOARDS_RASPBERRY_PI_1_MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint32_t vm_addr;
typedef uint32_t vm_offset;
typedef uint32_t vm_size;
typedef uint32_t pte_t;
typedef uint32_t pde_t;


// Constants
#define ROOT_PAGETABLES_CNT 1
#define ROOT_PAGETABLES_PDE_BASE 0

#define IO_PAGETABLES_CNT 16
#define IO_PAGETABLES_PDE_BASE 2560

#define KERNEL_PAGETABLES_CNT 512
#define KERNEL_PAGETABLES_PDE_BASE 2048

#define ROOT_CEILING_ADDR 0x00010000
#define KERNEL_BASE_VA 0x80000000
#define IOMAP_BASE_VA 0xA0000000

// Defines

#define PAGE_SIZE 4096
#define VPAGETABLE_SZ 4096
#define VPTE_TABLE_OFFS 1024
#define PAGEDIR_SZ 16384

#define N_PAGEDIR_PDE 4096
#define N_PAGETABLE_PTE 256

#define VPTE_TABLE_OFFS 1024

/*
 * L1 - Page Directory Entries
 */
#define L1_ADDR_BITS 0xfff00000 /* L1 PTE address bits */
#define L1_IDX_SHIFT 20
#define L1_TABLE_SIZE 0x4000 /* 16K */

#define L1_TYPE_INV 0x00  /* Invalid   */
#define L1_TYPE_C 0x01    /* Coarse    */
#define L1_TYPE_S 0x02    /* Section   */
#define L1_TYPE_F 0x03    /* Fine      */
#define L1_TYPE_MASK 0x03 /* Type Mask */

#define L1_S_B 0x00000004      /* Bufferable Section */
#define L1_S_C 0x00000008      /* Cacheable Section  */
#define L1_S_AP(x) ((x) << 10) /* Access Permissions */

#define L1_S_ADDR_MASK 0xfff00000 /* Address of Section  */
#define L1_C_ADDR_MASK 0xfffffc00 /* Address of L2 Table */

/*
 * L2 - Page Table Entries
 */
#define L2_ADDR_MASK 0xfffff000 /* L2 PTE Address mask of page */
#define L2_ADDR_BITS 0x000ff000 /* L2 PTE address bits */
#define L2_IDX_SHIFT 12
#define L2_TABLE_SIZE 0x0400 /* 1K */

#define L2_TYPE_INV 0x00 /* PTE Invalid     */
#define L2_NX 0x01       /* No Execute bit */
#define L2_TYPE_S 0x02   /* PTE ARMv6 4k Small Page  */

#define L2_B 0x00000004 /* Bufferable Page */
#define L2_C 0x00000008 /* Cacheable Page  */

#define L2_AP(x) ((x) << 4)  // 2 bit access permissions
#define L2_TEX(x) ((x) << 6) // 3 bit memory-access ordering
#define L2_APX (1 << 9)      // Access permissions (see table in arm manual)
#define L2_S (1 << 10)  // shared by other processors (used for page tables?)
#define L2_NG (1 << 11) // Non-Global (when set uses ASID)

//
#define C1_XP (1 << 23) // 0 subpage enabled (arm v5 mode)

/*
 * Access Permissions
 */
#define AP_W      0x01    /* Writable */
#define AP_U      0x02    /* User */

#define AP_KR     0x00    /* kernel read */
#define AP_KRW    0x01    /* kernel read/write */
#define AP_KRWUR  0x02    /* kernel read/write usr read */
#define AP_KRWURW 0x03    /* kernel read/write usr read/write */




size_t get_physical_mem_size(void);


#endif


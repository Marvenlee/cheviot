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

#ifndef MACHINE_BOARD_HAL_ARM_H
#define MACHINE_BOARD_HAL_ARM_H

#include <stdint.h>
#include <sys/types.h>


/*
 * types
 */
typedef volatile int spinlock_t;
typedef uint32_t vm_addr;
typedef uint32_t vm_offset;
typedef uint32_t vm_size;
typedef uint32_t pte_t;
typedef uint32_t pde_t;



/*
 * Page, Address Space and Pmap definitions
 */
#define PAGE_SIZE           4096
#define LARGE_PAGE_SIZE   	65536
#define VPAGETABLE_SZ       4096
#define VPTE_TABLE_OFFS     1024
#define PAGEDIR_SZ          16384

#define N_PAGEDIR_PDE       4096
#define N_PAGETABLE_PTE     256

#define BOOTLOADER_PAGETABLES_PDE_BASE    0
#define BOOTLOADER_PAGETABLES_CNT         1

#define KERNEL_PAGETABLES_PDE_BASE  2048
#define KERNEL_PAGETABLES_CNT       2047

#define IO_PAGETABLES_PDE_BASE      4095
#define IO_PAGETABLES_CNT			      1

#define VM_BOOTLOADER_CEILING		    		0x00010000

#define VM_USER_BASE_PAGETABLE_ALIGNED 	0x00000000
#define VM_USER_BASE      						 	0x00008000
#define VM_USER_CEILING   							0x7F000000

#define VM_KERNEL_BASE    							0x80000000
#define VM_IO_BASE 											0xFFF00000





/*
 * Instructions in hex
 */
#define LDR_PC_PC   (0xE59FF000)


/*
 * CPSR flags
 */
#define CPSR_N      (1 << 31)
#define CPSR_Z      (1 << 30)
#define CPSR_C      (1 << 29)
#define CPSR_V      (1 << 28)
#define CPSR_Q      (1 << 27)
#define CPSR_J      (1 << 24)
#define CPSR_E      (1 << 9)
#define CPSR_A      (1 << 8)
#define CPSR_I      (1 << 7)
#define CPSR_F      (1 << 6)
#define CPSR_T      (1 << 5)

#define CPSR_GE_MASK      (0x000F0000)
#define CPSR_MODE_MASK    (0x0000001F)

#define USR_MODE    0x10
#define FIQ_MODE    0x11
#define IRQ_MODE    0x12
#define SVC_MODE    0x13
#define ABT_MODE    0x17
#define UND_MODE    0x1B
#define SYS_MODE    0x1F

#define CPSR_DNM_MASK     (0x06F0FC00)
#define CPSR_USER_MASK    (CPSR_N | CPSR_Z | CPSR_C | CPSR_V | CPSR_Q)
#define CPSR_DEFAULT_BITS (CPSR_F)


/*
 * SCTLR Control Register flags
 */
#define SCTLR_TE        (1 << 30) /* Thumb Exception Enable */
#define SCTLR_AFE       (1 << 29) /* Access Flag Enable */
#define SCTLR_TRE       (1 << 28) /* TEX Remap Enable */
#define SCTLR_NMFI      (1 << 27) /* Non-maskable FIQ */
#define SCTLR_EE        (1 << 25)
#define SCTLR_HA        (1 << 17) /* Hardware Access AP0 flag (disable) */
#define SCTLR_RR        (1 << 14)
#define SCTLR_V         (1 << 13) /* High Vectors enabled at 0xFFFF0000 */
#define SCTLR_I         (1 << 12) /* Instruction cache enable */
#define SCTLR_Z         (1 << 11) /* Branch prediction enable*/
#define SCTLR_SW        (1 << 10) /* SWP/SWPB Enable */
#define SCTLR_CP15BEN   (1 << 5)  /* Enable cp15 barrier instructions */
#define SCTLR_C         (1 << 2)  /* Data cache enable */
#define SCTLR_A         (1 << 1)  /* Strict alignment of data enable */
#define SCTLR_M         (1 << 0)  /* MMU enable */


/*
 * Auxiliary Control Register (ACTLR) flags
 */
#define ACTLR_FW            (1 << 0)  /* Bit 0: Enable Cache/TLB maintenance broadcase */
#define ACTLR_L2_PREFECTH   (1 << 1)  /* Bit 1: L2 pre-fetch hint enable */
#define ACTLR_L1_PREFETCH   (1 << 2)  /* Bit 2: L1 Dside pre-fetch enable */
#define ACTLR_LINE_ZERO     (1 << 3)  /* Bit 3: Enable write full line zero mode */
                                           /* Bits 4-5: Reserved */
#define ACTLR_SMP           (1 << 6)  /* Bit 6: Cortex-A9 taking part in coherency */
#define ACTLR_EXCL          (1 << 7)  /* Bit 7: Exclusive cache bit */
#define ACTLR_ALLOC_1WAY    (1 << 8)  /* Bit 8: Allocation in 1-way cache only */
#define ACTLR_PARITY        (1 << 9)  /* Bit 9: Parity ON */


/*
 * Secure Configuration Register (SCR) flags
 */
#define SCR_NS          (1 << 0)  /* Bit 0: Non-secure */
#define SCR_IRQ         (1 << 1)  /* Bit 1: IRQs taken in Monitor mode */
#define SCR_FIQ         (1 << 2)  /* Bit 2: FIQs taken in Monitor mode */
#define SCR_EA          (1 << 3)  /* Bit 3: External aborts taken in Monitor mode */
#define SCR_FW          (1 << 4)  /* Bit 4: F bit writable */
#define SCR_AW          (1 << 5)  /* Bit 5: A bit writable */
#define SCR_NET         (1 << 6)  /* Bit 6: Not Early Termination */
#define SCR_SCD         (1 << 7)  /* Bit 7: Secure Monitor Call disable */
#define SCR_HCE         (1 << 8)  /* Bit 8: Hyp Call enable */
#define SCR_SIF         (1 << 9)  /* Bit 9: Secure state instruction fetches from Non-secure memory not permitted */


/*
 * L1 - Page Directory Entries
 */
#define L1_S_ADDR_MASK  0xFFF00000  /* Address of Section */
#define L1_C_ADDR_MASK  0xFFFFFC00  /* Address of L2 Table */

#define L1_ADDR_BITS    0xFFF00000  /* L1 PTE address bits */
#define L1_IDX_SHIFT    20
#define L1_TABLE_SIZE   0x4000      /* 16K */

#define L1_TYPE_INV   0x00          /* Invalid */
#define L1_TYPE_C     0x01          /* Coarse */
#define L1_TYPE_S     0x02          /* Section */
#define L1_TYPE_F     0x03          /* Fine */
#define L1_TYPE_MASK  0x03          /* L1 Entry Type Mask */

#define L1_S_AP2      (1 << 15)     /* Access Permissions for sections AP[2] */
#define L1_S_AP1      (1 << 11)     /* Access Permissions for sections AP[1] */
#define L1_S_AP0      (1 << 10)     /* Access Permissions for sections AP[0] or Access flag */
#define L1_S_C        (1 << 3)      /* Cacheable Section */
#define L1_S_B        (1 << 2)      /* Bufferable Section */


/* L1 Section permissions
 *
 * Constant           PL1(kernel)     PL0(user) 
 * -----------------------------------------------
 * L1_S_AP_NONE       no access       no access
 * L1_S_AP_RWK        read/write      no access
 * L1_S_AP_RWK_RU     read/write      read-only
 * L1_S_AP_RWKU       read/write      read/write
 * L1_S_AP_RK         read-only       no access
 * L1_S_AP_RKU        read-only       read-only
 */
#define L1_S_AP_NONE        (0)                               
#define L1_S_AP_RWK         (L1_S_AP0)
#define L1_S_AP_RWK_RU      (L1_S_AP1)
#define L1_S_AP_RWKU        (L1_S_AP1 | L1_S_AP0)
#define L1_S_AP_RK          (L1_S_AP2 | L1_S_AP0)
#define L1_S_AP_RKU         (L1_S_AP2 | L1_S_AP1 | L1_S_AP0)


/*
 * L2 - Page Table Entries
 */
#define L2_ADDR_MASK  0xFFFFF000  // L2 PTE mask of page address
#define L2_ADDR_BITS  0x000FF000  // L2 PTE address bits
#define L2_IDX_SHIFT  12
#define L2_TABLE_SIZE 0x0400      // Use 1KB, 256 entry page tables

#define L2_TYPE_MASK  0x03
#define L2_TYPE_INV   0x00        // PTE Invalid
#define L2_NX         0x01        // No Execute bit
#define L2_TYPE_S     0x02        // PTE ARMv6 4k Small Page

#define L2_NG       (1 << 11)     // Non-Global (when set uses ASID)
#define L2_S        (1 << 10)     // shared by other processors (used for page tables?)
#define L2_AP2      (1 << 9)      // Access permissions AP[2]
#define L2_TEX(x)   ((x) << 6)    // 3 bit memory-access ordering
#define L2_AP1      (1 << 5)      // Access permissions AP[1]
#define L2_AP0      (1 << 4)      // Access permissions AP[0] or Accessed flag
#define L2_C        (1 << 3)      // Cacheable Page
#define L2_B        (1 << 2)      // Bufferable Page


/* L2 page table entry permissions
 *
 * Constant           PL1(kernel)     PL0(user) 
 * -----------------------------------------------
 * L2_AP_NONE         no access       no access
 * L2_AP_RWK          read/write      no access
 * L2_AP_RWK_RU       read/write      read-only
 * L2_AP_RWKU         read/write      read/write
 * L2_AP_RK           read-only       no access
 * L2_S_AP_RKU        read-only       read-only
 */
#define L2_AP_NONE        (0)
#define L2_AP_RWK         (L2_AP0)
#define L2_AP_RWK_RU      (L2_AP1)
#define L2_AP_RWKU        (L2_AP1 | L2_AP0)
#define L2_AP_RK          (L2_AP2 | L2_AP0)
#define L2_AP_RKU         (L2_AP2 | L2_AP1 | L2_AP0)


/*
 * Data Fault Status Register (DFSR) flags
 */
#define DFSR_SD           (1 << 12)
#define DFSR_RW           (1 << 11)
#define DFSR_STS10        (1 << 10)
#define DFSR_DOMAIN(v)    ((v & 0x00f0) >> 4)
#define DFSR_STATUS(v)    (v & 0x000f)

#define DFSR_ALIGNMENT_FAULT			0x1
#define DFSR_PERMISSION_FAULT			0xD

/* 
 * Domain Access Control Register (DACR) flags
 */
#define DACR_MASK(d)      (3 << (d * 2))
#define DACR_SET(d, val)  (val << (d * 2))
#define DACR_NONE         (0)              // All accesses generate a domain fault
#define DACR_CLIENT       (1)              // Accesses checked against permissions TLB
#define DACR_MANAGER      (3)              // Accesses are not checked

#define DACR_NONE_ALL     (0x00000000)
#define DACR_CLIENT_ALL   (0x55555555)
#define DACR_MANAGER_ALL  (0xFFFFFFFF)


/*
 * Prototypes
 */
uint32_t hal_get_cpsr(void);
uint32_t hal_get_spsr(void);

uint32_t hal_get_far(void);
uint32_t hal_get_dfsr(void);
uint32_t hal_get_ifsr(void);

uint32_t hal_get_vbar(void);
void hal_set_vbar(uint32_t reg);

uint32_t hal_get_dacr(void);
void hal_set_dacr(uint32_t reg);

uint32_t hal_get_hcr(void);
void hal_set_hcr(uint32_t reg);

uint32_t hal_get_actlr(void);
void hal_set_actlr(uint32_t reg);

uint32_t hal_get_sctlr(void);
void hal_set_sctlr(uint32_t reg);
uint32_t hal_get_ttbcr(void);
void hal_set_ttbcr(uint32_t reg);  
uint32_t hal_get_ttbr0(void);
void hal_set_ttbr0(uint32_t reg);
uint32_t hal_get_ttbr1(void);
void hal_set_ttbr1(uint32_t reg);

uint32_t hal_get_tlb_type(void);
void hal_set_tlb_type(uint32_t reg);

void hal_isb(void);
void hal_dsb(void);
void hal_dmb(void);

void hal_invalidate_branch(void);
void hal_invalidate_icache(void);

void hal_invalidate_asid(uint32_t asid);
void hal_invalidate_tlb_va(uint32_t vaddr);
void hal_invalidate_tlb(void);

void hal_invalidate_dcache(void *start_vaddr, void *end_vaddr);
void hal_clean_dcache(void *start_vaddr, void *end_vaddr);
void hal_flush_dcache(void *start_vaddr, void *end_vaddr);


#endif

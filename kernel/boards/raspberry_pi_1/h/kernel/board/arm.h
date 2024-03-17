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

#ifndef MACHINE_BOARD_RASPBERRY_PI_1_ARM_H
#define MACHINE_BOARD_RASPBERRY_PI_1_ARM_H

#include <kernel/lists.h>
#include <kernel/types.h>
#include <stdint.h>
#include <machine/cheviot_hal.h>

/* Forward declarations
 */
struct UserContext;

/*
 * Arch types
 */

typedef volatile int spinlock_t;
typedef uint32_t vm_addr;
typedef uint32_t vm_offset;
typedef uint32_t vm_size;
typedef uint32_t pte_t;

typedef uint8_t     bits8_t;
typedef uint16_t    bits16_t;
typedef uint32_t    bits32_t;
typedef long long   uuid_t;
typedef uint32_t    context_word_t;
typedef uint32_t    int_state_t;


/*
 * Misc Addresses and registers
 */
#define VECTOR_TABLE_ADDR   0x00000000
#define LDR_PC_PC           0xE59FF000

/*
 * CPSR flags
 */
#define CPSR_N (1 << 31)
#define CPSR_Z (1 << 30)
#define CPSR_C (1 << 29)
#define CPSR_V (1 << 28)
#define CPSR_Q (1 << 27)
#define CPSR_J (1 << 24)
#define CPSR_GE_MASK (0x000F0000)
#define CPSR_E (1 << 9)
#define CPSR_A (1 << 8)
#define CPSR_I (1 << 7)
#define CPSR_F (1 << 6)
#define CPSR_T (1 << 5)
#define CPSR_MODE_MASK (0x0000001F)

#define CPSR_DNM_MASK 0x06F0FC00
#define CPSR_USER_MASK (CPSR_N | CPSR_Z | CPSR_C | CPSR_V | CPSR_Q)
#define CPSR_DEFAULT_BITS (CPSR_F)

/*
 * CPU Modes
 */
#define USR_MODE 0x10
#define FIQ_MODE 0x11
#define IRQ_MODE 0x12
#define SVC_MODE 0x13
#define ABT_MODE 0x17
#define UND_MODE 0x1b
#define SYS_MODE 0x1f

/*
 * Control Register flags
 */
#define C1_FA (1 << 29) // Force access bit
#define C1_TR (1 << 29) // TEX Remap enabled
#define C1_EE (1 << 25) // EE bit in CPSP set on exception
#define C1_VE (1 << 24) // VIC determines interrupt vectors
#define C1_XP (1 << 23) // 0 subpage enabled (arm v5 mode)
#define C1_U (1 << 22)  // Unaligned access enable
#define C1_V (1 << 13) // High Vectors enabled at 0xFFFF0000
#define C1_I (1 << 12) // Enable L1 Instruction cache
#define C1_Z (1 << 11) // Enable branch prediction
#define C1_C (1 << 2) // Enable L1 Data cache
#define C1_A (1 << 1) // Enable strict-alignment
#define C1_M (1 << 0) // Enable MMU

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
#define L2_ADDR_MASK  0xfffff000  // L2 PTE mask of page address
#define L2_ADDR_BITS  0x000ff000  // L2 PTE address bits
#define L2_IDX_SHIFT  12
#define L2_TABLE_SIZE 0x0400      // Use 1KB, 256 entry page tables

#define L2_TYPE_MASK  0x03
#define L2_TYPE_INV   0x00        // PTE Invalid
#define L2_NX         0x01        // No Execute bit
#define L2_TYPE_S     0x02        // PTE ARMv6 4k Small Page

#define L2_B 0x00000004           // Bufferable Page
#define L2_C 0x00000008           // Cacheable Page

#define L2_AP(x)  ((x) << 4)      // 2 bit access permissions
#define L2_TEX(x) ((x) << 6)      // 3 bit memory-access ordering
#define L2_APX  (1 << 9)          // Access permissions (see table in arm manual)
#define L2_S    (1 << 10)         // shared by other processors (used for page tables?)
#define L2_NG   (1 << 11)         // Non-Global (when set uses ASID)

/*
 * Access Permissions
 */
#define AP_W      0x01    /* Writable */
#define AP_U      0x02    /* User */

#define AP_KR     0x00    /* kernel read */
#define AP_KRW    0x01    /* kernel read/write */
#define AP_KRWUR  0x02    /* kernel read/write usr read */
#define AP_KRWURW 0x03    /* kernel read/write usr read/write */

/*
 * Short-hand for common AP_* constants.
 *
 * Note: These values assume the S (System) bit is set and
 * the R (ROM) bit is clear in CP15 register 1.
 */
#define AP_KR 0x00     // kernel read
#define AP_KRW 0x01    // kernel read/write
#define AP_KRWUR 0x02  // kernel read/write usr read
#define AP_KRWURW 0x03 // kernel read/write usr read/write

/*
 * DFSR register bits
 */
#define DFSR_SD (1 << 12)
#define DFSR_RW (1 << 11)
#define DFSR_STS10 (1 << 10)
#define DFSR_DOMAIN(v) ((v & 0x00f0) >> 4)
#define DFSR_STATUS(v) (v & 0x000f)

/*
 * General VM Constants
 */
#define PAGE_SIZE         4096
#define LARGE_PAGE_SIZE   65536
#define VM_KERNEL_BASE    0x80000000
#define VM_KERNEL_CEILING 0x8FFF0000
#define VM_USER_BASE      0x00400000
#define VM_USER_CEILING   0x7F000000

#define ROOT_CEILING_ADDR 0x00010000
#define KERNEL_BASE_VA    0x80000000
#define IOMAP_BASE_VA     0xA0000000

#define ROOT_PAGETABLES_CNT         1
#define ROOT_PAGETABLES_PDE_BASE    0

#define IO_PAGETABLES_CNT           16
#define IO_PAGETABLES_PDE_BASE      2560

#define KERNEL_PAGETABLES_CNT       512
#define KERNEL_PAGETABLES_PDE_BASE  2048

#define VPAGETABLE_SZ     4096
#define VPTE_TABLE_OFFS   1024
#define PAGEDIR_SZ        16384

#define N_PAGEDIR_PDE     4096
#define N_PAGETABLE_PTE   256

/*
 * PmapVPTE virtual page table flags
 */
#if 0 
#define VPTE_PHYS     (1 << 0)
#define VPTE_LAZY     (1 << 2)
#define VPTE_PROT_COW (1 << 3)
#define VPTE_PROT_R   (1 << 4)
#define VPTE_PROT_W   (1 << 5) 
#define VPTE_PROT_X   (1 << 6)
#define VPTE_ACCESSED (1 << 7) // FIXME: Should be part of Pageframe only
#define VPTE_DIRTY    (1 << 8) // FIXME: Should be part of Pageframe only
#define VPTE_WIRED    (1 << 9) // FIXME: Should be part of Pageframe only
#define VPTE_PRESENT  (1 << 10)
#endif

/*
 */
LIST_TYPE(Pmap, pmap_list_t, pmap_list_link_t);
LIST_TYPE(PmapVPTE, pmap_vpte_list_t, pmap_vpte_list_link_t);

struct Pmap
{
  uint32_t *l1_table; // Page table
};

struct PmapVPTE
{
  pmap_vpte_list_link_t link;
  uint32_t flags;
} __attribute__((packed));

struct PmapPageframe
{
  pmap_vpte_list_t vpte_list;
};


// Macros
#define VirtToPhys(va) ((vm_addr)va & 0x7FFFFFFF)
#define PhysToVirt(pa) ((vm_addr)pa | 0x80000000)

//#define AtomicSet(var_name, val) *var_name = val;
//#define AtomicGet(var_name) var_name

// Prototypes
void reset_vector(void);
void undef_instr_vector(void);
void swi_vector(void);
void prefetch_abort_vector(void);
void data_abort_vector(void);
void reserved_vector(void);
void irq_vector(void);
void fiq_vector(void);

void DeliverException(void);

void CheckSignals(struct UserContext *context);

void PrefetchAbortHandler(struct UserContext *context);
void DataAbortHandler(struct UserContext *context);
void UndefInstrHandler(struct UserContext *context);
void FiqHandler(void);

void init_interrupt_controller(void);
void interrupt_handler(struct UserContext *context);
void interrupt_top_half(void);
void interrupt_top_half_timer(void);
void save_pending_interrupts(void);
bool check_pending_interrupt(int irq);
void clear_pending_interrupt(int irq);
uint32_t get_pending_interrupt_word(int irq);

void PrintUserContext(struct UserContext *uc);


// Move to HAL library 

int_state_t DisableInterrupts(void);
void RestoreInterrupts(int_state_t state);
void EnableInterrupts(void);





void FPUSwitchState(void);


void SpinLock(spinlock_t *spinlock);
void SpinUnlock(spinlock_t *spinlock);



void PmapPageFault(void);
uint32_t *PmapGetPageTable(struct Pmap *pmap, int pde_idx);



#endif

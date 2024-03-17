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

#ifndef MACHINE_BOARD_RASPBERRY_PI_4_ARM_H
#define MACHINE_BOARD_RASPBERRY_PI_4_ARM_H

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
void clear_pending_interrupt(uint32_t irq_ack);
uint32_t get_pending_interrupt_word(int irq);

void PrintUserContext(struct UserContext *uc);

int_state_t DisableInterrupts(void);
void RestoreInterrupts(int_state_t state);
void EnableInterrupts(void);

void FPUSwitchState(void);

void SpinLock(spinlock_t *spinlock);
void SpinUnlock(spinlock_t *spinlock);

void PmapPageFault(void);
uint32_t *PmapGetPageTable(struct Pmap *pmap, int pde_idx);



#endif

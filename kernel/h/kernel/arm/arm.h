#ifndef KERNEL_ARM_ARM_H
#define KERNEL_ARM_ARM_H


#include <kernel/types.h>
#include <kernel/lists.h>


/*
 * Forward declarations
 */

// struct Process;




/*
 * Arch types
 */


typedef volatile int         spinlock_t;
typedef uint32      vm_addr;
typedef uint32      vm_offset;
typedef uint32      vm_size;
typedef uint32      pte_t;
typedef uint32      iflag_t;



// Should pack fields together

//#define PAGETABLE_POOL_SIZE   8388608
#define VPAGETABLE_SZ       4096
#define VPTE_TABLE_OFFS     1024
#define PAGEDIR_SZ          16384

#define N_PAGEDIR_PDE      (4096)     
#define N_PAGETABLE_PTE      (256)     

#define KERNEL_BASE     0x80000000
#define KERNEL_CEILING  0xF0000000


/*
 *
 */

#define VECTOR_TABLE_ADDR       0x00000000
#define LDR_PC_PC               0xE59FF000


/*
 * CPSR flags
 */

#define CPSR_N                  (1<<31)
#define CPSR_Z                  (1<<30)
#define CPSR_C                  (1<<29)
#define CPSR_V                  (1<<28)
#define CPSR_Q                  (1<<27)
#define CPSR_J                  (1<<24)
#define CPSR_GE_MASK            (0x000F0000)
#define CPSR_E                  (1<<9)
#define CPSR_A                  (1<<8)
#define CPSR_I                  (1<<7)
#define CPSR_F                  (1<<6)
#define CPSR_T                  (1<<5)
#define CPSR_MODE_MASK          (0x0000001F)

#define CPSR_DNM_MASK           0x06F0FC00
#define CPSR_USER_MASK          (CPSR_N | CPSR_Z | CPSR_C | CPSR_V | CPSR_Q)
#define CPSR_DEFAULT_BITS       ( CPSR_F )



/*
 * CPU Modes
 */

#define USR_MODE    0x10
#define FIQ_MODE    0x11
#define IRQ_MODE    0x12
#define SVC_MODE    0x13
#define ABT_MODE    0x17
#define UND_MODE    0x1b
#define SYS_MODE    0x1f

/*
 *
 */

#define C1_XP           (1<<23)       // 0 subpage enabled (arm v5 mode)




/*
 * virtual page table flags
 */
 
#define VPTE_PHYS            (1 << 0)

#define VPTE_LAZY          (1 << 2)
#define VPTE_PROT_COW      (1 << 3)
#define VPTE_PROT_R        (1 << 4)
#define VPTE_PROT_W        (1 << 5)
#define VPTE_PROT_X        (1 << 6)
#define VPTE_ACCESSED      (1 << 7)   // Should be part of PF
#define VPTE_DIRTY         (1 << 8)  // Should be part of PF

#define VPTE_WIRED         (1 << 9)   // Should be part of PF

#define VPTE_PRESENT       (1 << 10)


#define VPTE_TABLE_OFFS 1024



/*
 * L1 - Page Directory Entries
 */

#define L1_ADDR_BITS    0xfff00000  /* L1 PTE address bits */
#define L1_IDX_SHIFT    20
#define L1_TABLE_SIZE   0x4000      /* 16K */

#define L1_TYPE_INV     0x00        /* Invalid   */
#define L1_TYPE_C       0x01        /* Coarse    */
#define L1_TYPE_S       0x02        /* Section   */
#define L1_TYPE_F       0x03        /* Fine      */
#define L1_TYPE_MASK    0x03        /* Type Mask */

#define L1_S_B          0x00000004  /* Bufferable Section */
#define L1_S_C          0x00000008  /* Cacheable Section  */
#define L1_S_AP(x)      ((x) << 10) /* Access Permissions */

#define L1_S_ADDR_MASK  0xfff00000  /* Address of Section  */
#define L1_C_ADDR_MASK  0xfffffc00  /* Address of L2 Table */

/*
 * L2 - Page Table Entries
 */

#define L2_ADDR_BITS    0x000ff000  /* L2 PTE address bits */
#define L2_IDX_SHIFT    12
#define L2_TABLE_SIZE   0x0400      /* 1K */

#define L2_TYPE_INV     0x00        /* PTE Invalid     */
#define L2_TYPE_L       0x01        /* PTE Large page  */
#define L2_TYPE_S       0x02        /* PTE Small Page  */
#define L2_TYPE_MASK    0x03

#define L2_B        0x00000004      /* Bufferable Page */
#define L2_C        0x00000008      /* Cacheable Page  */
#define L2_AP(x)    (((x) << 4) | ((x) << 6) | ((x) << 8) | ((x) << 10))


/*
 * Access Permissions
 */

#define AP_W            0x01        /* Writable */
#define AP_U            0x02        /* User     */



/*
 * Short-hand for common AP_* constants.
 *
 * Note: These values assume the S (System) bit is set and
 * the R (ROM) bit is clear in CP15 register 1.
 */
#define AP_KR       0x00        /* kernel read */
#define AP_KRW      0x01        /* kernel read/write */
#define AP_KRWUR    0x02        /* kernel read/write usr read */
#define AP_KRWURW   0x03        /* kernel read/write usr read/write */




/*
 * DFSR register bits
 */

#define DFSR_SD         (1<<12)
#define DFSR_RW         (1<<11)
#define DFSR_STS10      (1<<10)
#define DFSR_DOMAIN(v)  ((v & 0x00f0) >> 4)
#define DFSR_STATUS(v)  (v & 0x000f)






/*
 * General VM Constants
 */

#define PAGE_SIZE                   4096
#define LARGE_PAGE_SIZE             65536
#define VM_KERNEL_BASE              0x00000000
#define VM_KERNEL_CEILING           0x00800000
#define VM_USER_BASE                0x00800000
#define VM_USER_CEILING             0xFFFF0000


#define NPMAP                       64
#define NPDE                        16         /* Pagetables per process */
#define PMAP_SIZE                   (L2_TABLE_SIZE * NPDE)





/*
 * Pmap (Page table Map) is the CPU-dependent part of the Virtual Memory
 * Management routines.  Converts the CPU-independent MemArea and Segment
 * structures into something a CPU can understand, page tables!
 *
 * We use a single 16k page directory with 4096 entries.  Each process
 * is reserved 16k of kernel memory containing 16 1k page tables that
 * hold 256 entries.  These 16 page tables are maintained in a simple
 * circular buffer,  discarding the least recently created page table
 * to make room for a new one.
 *
 * At any one time there will be a maximum of 16 valid entries in the
 * page directory.
 *
 * On a task switch the current 16 entries in the page directory are
 * marked as invalid and the 16 page tables of the new process are
 * entered into the page directory.
 */

#define PMAP_MAP_VIRTUAL            0
#define PMAP_MAP_PHYSICAL           1


struct Process;

#define L2_PAGETABLES		16



struct Pmap
{
    uint32 *l1_table;             // Page table    
};


struct PmapVPTE
{
    LIST_ENTRY (PmapVPTE) link;
    uint32 flags;
} __attribute__ ((packed));


struct PmapPageframe
{
    LIST (PmapVPTE) vpte_list;
};



LIST_TYPE (Pmap)     pmap_list_t;





// Move to macros

#define VirtToPhys(va) ((vm_addr)va & 0x7FFFFFFF)
#define PhysToVirt(pa) ((vm_addr)pa | 0x80000000)



#define AtomicSet(var_name, val)           \
                *var_name = val;


#define AtomicGet(var_name)                 \
                    var_name                 



void SpinLock (spinlock_t *spinlock);
void SpinUnlock (spinlock_t *spinlock);



void reset_vector (void);
void undef_instr_vector (void);
void swi_vector (void);
void prefetch_abort_vector (void);
void data_abort_vector (void);
void reserved_vector (void);
void irq_vector (void);
void fiq_vector (void);

void DeliverException (void);

void PrefetchAbortHandler (void);
void DataAbortHandler (void);
void UndefInstrHandler (void);
void FiqHandler (void);

void InterruptTopHalf (void);
void InterruptBottomHalf (void);




void InitTimer (void);
void InitInterruptController (void);

vm_addr GetFAR (void);
bits32_t GetCPSR(void);
bits32_t GetDFSR (void);
bits32_t GetIFSR (void);
bits32_t GetSPSR (void);

void MemoryBarrier (void);
void DataCacheFlush (void);
void SyncBarrier (void);
void DataSyncBarrier (void);
void FlushCaches (void);
void InvalidateTLB (void);
void EnableL1Cache (void);
void DisableL1Cache (void);
void EnablePaging (vm_addr pagedir, bits32_t flags);
void FPUSwitchState (void);

void DisableInterrupts(void);
void EnableInterrupts(void);

SYSCALL int MaskInterrupt (int irq);
SYSCALL int UnmaskInterrupt (int irq);
void EnableIRQ (int irq);
void DisableIRQ (int irq);


// GetPagecache();
// ReleasePagecache();

// pmap pagetable stuff built ontop of getpagecache/releasepagecache

void PmapPageFault (void);
uint32 *PmapGetPageTable (struct Pmap *pmap, int pde_idx);




bits32_t GetCtrl(void);
void SetCtrl (bits32_t reg);

#endif

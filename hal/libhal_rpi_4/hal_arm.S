
.global hal_enable_interrupts
.global hal_disable_interrupts
.global hal_get_cpsr
.global hal_get_spsr
.global hal_get_far
.global hal_get_dfsr
.global hal_get_ifsr
.global hal_get_dacr
.global hal_set_dacr
.global hal_get_hcr
.global hal_set_hcr
.global hal_get_vbar
.global hal_set_vbar
.global hal_get_tlb_type
.global hal_set_tlb_type

.global hal_get_sctlr
.global hal_set_sctlr  
.global hal_get_ttbcr
.global hal_set_ttbcr  
.global hal_get_ttbr0 
.global hal_set_ttbr0
.global hal_get_ttbr1 
.global hal_set_ttbr1

.global hal_isb
.global hal_dsb
.global hal_dmb

.global hal_invalidate_branch
.global hal_invalidate_icache
.global hal_invalidate_dcache
.global hal_clean_dcache
.global hal_flush_dcache
.global hal_invalidate_asid
.global hal_invalidate_tlb_va
.global hal_invalidate_tlb


.section .text

hal_enable_interrupts:
    mrs r0, cpsr        
    bic r0, #0x80
    msr cpsr_c, r0
    bx lr

hal_disable_interrupts:
    mrs r0, cpsr
    orr r0, #0x80
    msr cpsr_c, r0
    bx lr

hal_get_cpsr:
    mrs r0, cpsr
    bx lr

hal_get_spsr:
    mrs r0, spsr
    bx lr

hal_get_far:
    mrc p15, 0, r0, c6, c0, 0
    bx lr

hal_get_dfsr:
    mrc p15, 0, r0, c5, c0, 0 
    bx lr

hal_get_ifsr:
    mrc p15, 0, r0, c5, c0, 1 
    bx lr

hal_get_dacr:
    mrc p15, 0, r0, c3, c0, 0;
    bx lr
    
hal_set_dacr:
    mcr p15, 0, r0, c3, c0, 0;
    bx lr

hal_set_hcr:    
    mcr p15, 4, r0, c1, c1, 0
    bx lr
    
hal_get_hcr:
    mrc p15, 4, r0, c1, c1, 0
    bx lr
    
hal_get_tlb_type:
    mrc p15, 0, r0, c0, c0, #0
    bx lr
    
hal_set_tlb_type:
    mcr p15, 0, r0, c0, c0, #0
    bx lr

hal_get_vbar:
    mrc p15, 0, r0, c12, c0, #0
    bx lr

hal_set_vbar:
    mcr p15, 0, r0, c12, c0, #0
    bx lr

hal_get_sctlr:
  mrc p15, 0, r0, c1, c0, 0 // Read SCTLR into R0
  bx lr

hal_set_sctlr:  
  mcr p15, 0, r0, c1, c0, 0 // Write R0 to SCTLR
  bx lr

hal_get_ttbcr:
  mrc p15, 0, r0, c2, c0, 2 // Read TTBCR into R0
  bx lr

hal_set_ttbcr:  
  mcr p15, 0, r0, c2, c0, 2 // Write R0 to TTBCR
  bx lr
  
hal_get_ttbr0: 
  mrc p15, 0, r0, c2, c0, 0 // Read 32-bit TTBR0 into R0
  bx lr
  
hal_set_ttbr0:
  mcr p15, 0, r0, c2, c0, 0 // Write R0 to 32-bit TTBR0  
  bx lr

hal_get_ttbr1: 
  mrc p15, 0, r0, c2, c0, 1 // Read 32-bit TTBR1 into R0
  bx lr
  
hal_set_ttbr1:
  mcr p15, 0, r0, c2, c0, 1 // Write R0 to 32-bit TTBR1  
  bx lr


/* @brief   ISB helper macro
 *
 */
.macro isb
    mcr p15, 0, r0, c7, c5, 4;      // r0 is ignored
.endm


/* @brief   DSB helper macro
 *
 */    
.macro dsb
    mcr p15, 0, r0, c7, c10, 4      // r0 is ignored
.endm


/* @brief   Instruction Synchronization Barrier
 *
 */
hal_isb:
    isb
    bx lr


/* @brief   Data Synchronization Barrier
 *
 */
hal_dsb:
    dsb
    bx lr    


/* @brief   Data Memory Barrier
 *
 */
hal_dmb:
    mov r0, #0
    mcr p15, 0, r0, c7, c10, 5
    bx lr


/* @brief   Invalidate entries in the Branch Target Buffer
 *
 * Performs the BPIALL operation
 */
hal_invalidate_branch:
	dsb
	mov r0, #0
	mcr p15, 0, r0, c7, c5, 6
	dsb
	isb
	bx lr


/* @brief   Invalidate the current CPU's instruction cache
 * 
 * Performs the ICIALLU operation
 */
hal_invalidate_icache:
	dsb
	mov r0, #0
	mcr p15, 0, r0, c7, c5, 0
	dsb
	isb
	bx lr


/* @brief   Invalidate the Address Space ID (ASID) register
 *
 * Performs the UTLBIASID operation
 */
hal_invalidate_asid:
	dsb
	and r0, r0, #0xFF
	mcr p15, 0, r0, c8, c7, 2
	dsb
	isb
	bx lr


/* @brief   Invalidate a single TLB based on virtual address
 * 
 * Performs the UTLBIMVA operation
 */
hal_invalidate_tlb_va:
	dsb
	mcr p15, 0, r0, c8, c7, 1
	dsb
	isb
	bx lr


/* @brief   Invalidates the entire TLB
 *
 * Performs the UTLBIALL operation
 */
hal_invalidate_tlb:
	dsb
	mcr p15, 0, r0, c8, c7, 0
	dsb
	isb
	bx lr


/* @brief   cache op helper macro
 */
.macro calc_cache_lines
	mrc p15, 0, r3, c0, c0, 1  /* Read the Cache Type Register */
	lsr r3, r3, #16            /* DMinLine value */
	and r3, r3, #0xf
	mov r2, #4
	mov r2, r2, lsl r3         /* Cache line size in bytes */
	sub r3, r2, #1             /* Cache line size mask */
	bic r0, r0, r3
.endm


/* @brief   Invalidate the current CPU's data cache
 *
 * void hal_invalidate_dcache(void *start_vaddr, void *end_vaddr);
 *
 * Invalidates the cache content that are between addresses
 * start_vaddr and end_vaddr.
 *
 * Call this after a DMA operation writes data to main memory
 * so that the CPU does not pick up stale data. Performs the DCIMVAC
 * operation on this memory range.
 *
 * This is based on Phoenix-RTOS source
 */
hal_invalidate_dcache:
	dsb
	calc_cache_lines
inval_line:
	mcr p15, 0, r0, c7, c6, 1
	add r0, r0, r2
	cmp r0, r1
	blo inval_line
	dsb
	isb
	bx lr


/* @brief   Clean the data cache
 *
 * void hal_clean_dcache(void *start_vaddr, void *end_vaddr);
 *
 * Cleans the cache content that are between addresses
 * start_vaddr and end_vaddr. Performs the DCCMVAC operation
 * on this memory range.
 *
 * This is based on Phoenix-RTOS source
 */
hal_clean_dcache:
	dsb
  calc_cache_lines
clean_line:
	mcr p15, 0, r0, c7, c10, 1
	add r0, r0, r2
	cmp r0, r1
	blo clean_line
	dsb
	isb
	bx lr


/* @brief   Flush the data cache
 *
 * void hal_flush_dcache(void *start_vaddr, void *end_vaddr);
 *
 * Flushes the cache content that are between addresses
 * start_vaddr and end_vaddr.
 *
 * Call this prior to a DMA operation that reads data so that
 * data is written to main memory before a DMA device reads
 * from it. Performs the DCCIMVAC operation on this memory range.
 *
 * This is based on Phoenix-RTOS source
 */
hal_flush_dcache:
	dsb
	calc_cache_lines
flush_line:
	mcr p15, 0, r0, c7, c14, 1
	add r0, r0, r2
	cmp r0, r1
	blo flush_line
	dsb
	isb
	bx lr



.end


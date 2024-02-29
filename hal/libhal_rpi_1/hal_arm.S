
.global hal_enable_interrupts
.global hal_disable_interrupts

.global hal_get_ctrl
.global hal_set_ctrl
.global hal_get_cpsr
.global hal_get_spsr
.global hal_get_far
.global hal_get_dfsr
.global hal_get_ifsr
.global hal_get_dacr
.global hal_set_dacr
.global hal_get_vbar
.global hal_set_vbar
.global hal_get_tlb_type
.global hal_get_tlb_type

.global hal_isb
.global hal_dsb
.global hal_dmb
.global hal_memory_barrier
.global hal_sync_barrier
.global hal_data_sync_barrier

.global hal_data_cache_flush
.global hal_flush_all_caches
.global hal_flush_caches
.global hal_enable_l1_cache
.global hal_disable_l1_cache
.global hal_enable_paging
.global hal_set_page_directory
.global hal_invalidate_tlb



.section .text


hal_get_ctrl:
    mrc p15, 0, r0, c1, c0, #0
    bx lr

hal_set_ctrl:
    mcr p15, 0, r0, c1, c0, #0
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

hal_isb:
    mov r0, #0;
    mcr p15, #0, r0, c7, c5, #4;
    bx lr

hal_dsb:
    mcr p15, 0, r12, c7, c10, 4
    bx lr    

hal_dmb:
    mcr p15, 0, r12, c7, c10, 5
    bx lr

hal_flush_cache_line:
    mcr p15, 0, r0, c7, c6, #1
    bx lr

hal_memory_barrier:
    mov r0, #0
    mcr p15, #0, r0, c7, c10, #5
    bx lr

hal_sync_barrier:
    mov r0, #0
    mcr p15, #0, r0, c7, c10, #4
    bx lr

hal_data_sync_barrier:
    stmfd sp!, {r0-r8,r12,lr}
    mcr p15, #0, ip, c7, c5, #0
    mcr p15, #0, ip, c7, c5, #6
    mcr p15, #0, ip, c7, c10, #4
    mcr p15, #0, ip, c7, c10, #4
    ldmfd sp!, {r0-r8,r12,pc}
    
hal_data_cache_flush:
    mov r0, #0
    mcr p15, #0, r0, c7, c14, #0
    bx lr

hal_flush_all_caches:
		mov	r1, #0
		mcr	p15, 0, r1, c7, c14, 0	@ clean+invalidate D
		mcr	p15, 0, r1, c7, c5, 0	@ invalidate I+BTB
		mcr	p15, 0, r1, c7, c15, 0	@ clean+invalidate unified
		mcr	p15, 0, r1, c7, c10, 4	@ drain WB
		mov	pc, lr

hal_flush_caches:
    mov r0, #0
    mcr p15, 0, r0, c7, c14, 0;
    mcr p15,0,r0,c7,c7,0        // invalidate caches
    mcr p15,0,r0,c8,c7,0        // invalidate tlb
    bx lr
    
hal_enable_l1_cache:
    mov r0, #0
    mcr p15, 0, r0, c7, c7, 0   // invalidate caches
    mcr p15, 0, r0, c8, c7, 0   // invalidate tlb
    mrc p15, 0, r0, c1, c0, 0
    orr r0,r0,#0x1000           // instruction
    orr r0,r0,#0x0004           // data
    mcr p15, 0, r0, c1, c0, 0
    bx lr

hal_disable_l1_cache:
    mrc p15, 0, r0, c1, c0, 0
    bic r0,r0,#0x1000 ;@ instruction
    bic r0,r0,#0x0004 ;@ data
    mcr p15, 0, r0, c1, c0, 0
    bx lr

hal_enable_paging:
    mov r2,#0
    mcr p15, 0, r2, c7, c14, 0
    mcr p15,0,r2,c7,c7,0        // invalidate caches
    mcr p15,0,r2,c8,c7,0        // invalidate tlb
    mvn r2,#0
    mcr p15,0,r2,c3,c0,0        // domain
    mcr p15,0,r0,c2,c0,0        // tlb base
    mcr p15,0,r0,c2,c0,1        // tlb base
    mrc p15,0,r2,c1,c0,0
    orr r2,r2,r1
    mcr p15,0,r2,c1,c0,0
    bx lr

hal_set_page_directory:
    mov r2,#0
    
    // FIXME: SetPageDirectory - Should we be invalidating cache?
    // mcr p15,0,r2,c7,c7,0     // invalidate caches
    mcr p15,0,r2,c8,c7,0        // invalidate tlb
    
    mcr p15,0,r0,c2,c0,0        // tlb base
    mcr p15,0,r0,c2,c0,1        // tlb base
    bx lr

hal_invalidate_tlb:
    mov r0,#0
    mcr p15,0,r0,c8,c7,0
    bx lr

hal_enable_interrupts:
    push {r0}   
    mrs r0, cpsr        
    bic r0, #0x80
    msr cpsr_c, r0
    pop {r0}
    bx lr

hal_disable_interrupts:
    push {r0}
    mrs r0, cpsr
    orr r0, #0x80
    msr cpsr_c, r0
    pop {r0}
    bx lr



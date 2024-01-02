.global GetCPSR
.global GetSPSR
.global EnableInterrupts
.global DisableInterrupts
.global GetCtrl
.global SetCtrl
.global EnablePaging
.global isb
.global dsb
.global dmb


.section .text

GetCPSR:
    mrs r0, cpsr
    bx lr

GetSPSR:
    mrs r0, spsr
    bx lr


GetCtrl:
    mrc p15, 0, r0, c1, c0, #0
    bx lr

SetCtrl:
    mcr p15, 0, r0, c1, c0, #0
    bx lr

EnablePaging:
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



.macro MEnableInterrupts
    push {r0}   
    mrs r0, cpsr        
    bic r0, #0x80
    msr cpsr_c, r0
    pop {r0}
.endm


.macro MDisableInterrupts
    push {r0}
    mrs r0, cpsr
    orr r0, #0x80
    msr cpsr_c, r0
    pop {r0}
.endm


DisableInterrupts:
    MDisableInterrupts
    bx lr

EnableInterrupts:
    MEnableInterrupts
    bx lr

isb:
    mcr p15, 0, r12, c7, c5, 4
    bx lr

dsb:
    mcr p15, 0, r12, c7, c10, 4
    bx lr    

dmb:
    mcr p15, 0, r12, c7, c10, 5
    bx lr

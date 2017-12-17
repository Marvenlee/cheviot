
.global GetCtrl
.global SetCtrl
.global EnablePaging

.section .text

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





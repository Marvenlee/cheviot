    
.global isb
.global dsb
.global dmb

.section .text

isb:
    mcr p15, 0, r12, c7, c5, 4
    bx lr

dsb:
    mcr p15, 0, r12, c7, c10, 4
    bx lr    

dmb:
    mcr p15, 0, r12, c7, c10, 5
    bx lr


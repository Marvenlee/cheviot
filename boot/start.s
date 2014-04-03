.extern Main
.global mailbuffer


// Entry point into bootloader

.section .init
.globl _start
_start:

   ldr   r3, =_bss_end
   ldr   r2, =_bss_start
   cmp   r2, r3
   bcs   2f
   sub   r3, r2, #1
   ldr   r1, =_bss_end
   sub   r1, r1, #1
   mov   r2, #0
1:   strb   r2, [r3, #1]!
   cmp   r3, r1
   bne   1b

2:
#	mrc p15, 0, r0, c1, c0, 2
#	orr r0, r0, #0xC00000            /* double precision */
#	mcr p15, 0, r0, c1, c0, 2
#	mov r0, #0x40000000
#	fmxr fpexc,r0
	
	
	ldr sp, =istack_top
	b Main









.section .data
.balign 16

mailbuffer:
.skip 256


istack:
.skip 4096
istack_top:
.skip 64



	
	


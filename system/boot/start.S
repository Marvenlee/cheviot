.extern Main
.global mailbuffer
.global rootfs_image_offset
.global rootfs_image_size


// Entry point into bootloader
// We have 2 variables that are overwritten by mkifs tool which stores the offset
// and size of the IFS Initial File System.
// The IFS image is copied to 64k, out of the way of the BSS section which we
// must clear.

.section .init
.global _start
_start:
   b skip_magic
.balign 16

rootfs_image_offset:
    .long 0xdeadbeef
rootfs_image_size:
    .long 0xcafef00d

.balign 16

skip_magic:
 
 	
	ldr sp, =root_stack_top
	b Main




# tHIS should have memmoved (backwards) as overlapping area 
    ldr r3, =rootfs_image_size
    ldr r2, [r3]
    ldr r3, =rootfs_image_offset
    add r3, #0x8000
    ldr r1, [r3]
    ldr r3, =0x10000
    ldr r0, [r3]

copyifs:
    cmp r2, #0
    beq copyifsdone
    ldrb r3, [r1], #1
    strb r3, [r0], #1
    sub r2, #1
    b copyifs

copyifsdone:
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
	
	
#	ldr sp, =root_stack_top
#	b Main


.section .data
.balign 16
root_stack:
.skip 4096
root_stack_top:


mailbuffer:
.skip 256



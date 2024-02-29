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

.section .data
.balign 16
root_stack:
.skip 4096
root_stack_top:

// Is this needed ?
mailbuffer:
.skip 256



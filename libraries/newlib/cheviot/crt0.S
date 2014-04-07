#include "newlib.h"
#include "arm.h"


/* ANSI concatenation macros.  */
#define CONCAT(a, b) CONCAT2(a, b)
#define CONCAT2(a, b) a ## b

#ifdef __USER_LABEL_PREFIX__
#define FUNCTION( name ) CONCAT (__USER_LABEL_PREFIX__, name)
#else
#error __USER_LABEL_PREFIX is not defined
#endif


#ifdef HAVE_INITFINI_ARRAY
#define _init	__libc_init_array
#define _fini	__libc_fini_array
#endif

#if defined(__ARM_EABI__) && defined(__thumb__) && !defined(__thumb2__)
/* For Thumb1 we need to force the architecture to be sure that we get the
   correct attributes on the object file; otherwise the assembler will get
   confused and mark the object as being v6T2.  */
#if defined(__ARM_ARCH_4T__)
	.arch armv4t
#elif defined(__ARM_ARCH_5T__) || defined(__ARM_ARCH_5TE__)
	/* Nothing in this object requires higher than v5.  */
	.arch armv5t
#elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) \
	|| defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) \
	|| defined(__ARM_ARCH_6ZK__)
	/* Nothing in this object requires higher than v6.  */
	.arch armv6
#elif defined(__ARM_ARCH_6M__)
#ifdef ARM_RDP_MONITOR
	/* Object file uses SVC, so mark as v6s-m.  */
	.arch armv6s-m
#else
	.arch armv6-m
#endif
#endif
#endif



/* .text is used instead of .section .text so it works with arm-aout too.  */
	.text
	.syntax unified
#ifdef THUMB_V7_V6M
	.thumb
.macro FUNC_START name
	.global \name
	.thumb_func
\name:
.endm	
#else
	.code 32
.macro FUNC_START name
	.global	\name
\name:
.endm	
#endif

.macro indirect_call reg
#ifdef HAVE_CALL_INDIRECT
	blx \reg
#else
	mov	lr, pc
	mov	pc, \reg
#endif
.endm






.align 	0

	FUNC_START	_mainCRTStartup
	FUNC_START	_start
	FUNC_START	start


#if defined(__ELF__) && !defined(__USING_SJLJ_EXCEPTIONS__)
	/* Annotation for EABI unwinding tables.  */
	.fnstart
#endif








	/* Setup a default stack-limit in-case the code has been
	   compiled with "-mapcs-stack-check".  Hard-wiring this value
	   is not ideal, since there is currently no support for
	   checking that the heap and stack have not collided, or that
	   this default 64k is enough for the program being executed.
	   However, it ensures that this simple crt0 world will not
	   immediately cause an overflow event:  */

#ifdef __ARM_ARCH_6M__
	movs	r2, #64
	lsls	r2, r2, #10
	subs	r2, r3, r2
	mov	sl, r2
#else
	sub	sl, r3, #64 << 10	/* Still assumes 256bytes below sl.  */
#endif




	
#if !defined (ARM_RDP_MONITOR) && !defined (ARM_RDI_MONITOR)
/* Changes by toralf: Taken from libgloss/m68k/crt0.S
   initialize target specific stuff. Only execute these
   functions it they exist.  */
	ldr	r3, .Lhwinit
	cmp	r3, #0
	beq	.LC24
	indirect_call r3
.LC24:	
	ldr	r3, .Lswinit
	cmp	r3, #0
	beq	.LC25
	indirect_call r3
.LC25:	

#endif
	
	/* Ensure doubleword stack alignment.  */

//	bic	sp, sp, #7





#ifdef __USES_INITFINI__
	movs	r4, r0
	movs	r5, r1
	ldr	r0, .Lfini
	bl	FUNCTION (atexit)
	bl	FUNCTION (_init)
	movs	r0, r4
	movs	r1, r5
#endif	
	bl	FUNCTION (main)
	bl	FUNCTION (exit)		/* Should not return.  */









.balign 16
	
.Lhwinit:	
	.word	FUNCTION (hardware_init_hook)
.Lswinit:
	.word	FUNCTION (software_init_hook)

	/* Set up defaults for the above variables in the form of weak symbols
	   - so that application will link correctly, and get value 0 in
	   runtime (meaning "ignore setting") for the variables, when the user
	   does not provide the symbols. (The linker uses a weak symbol if,
	   and only if, a normal version of the same symbol isn't provided
	   e.g. by a linker script or another object file).  */	

	.weak __stack
	.weak FUNCTION (hardware_init_hook) 
	.weak FUNCTION (software_init_hook)





#if defined(__ELF__) && !defined(__USING_SJLJ_EXCEPTIONS__)
	/* Protect against unhandled exceptions.  */
	.cantunwind
	.fnend
#endif


.LC1:
	.word	__bss_start__
.LC2:
	.word	__bss_end__


#ifdef __USES_INITFINI__
.Lfini:
	.word	FUNCTION(_fini)
#endif




/*
 * Copyright 2014  Marven Gilhespie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Kernel entry point and general ARM CPU functions
 */ 

#include <kernel/arm/arm.i>
#include <kernel/arm/task.i>
#include <kernel/arm/macros.i>

.global FlushAllCaches
.global FlushCacheLine

.global _start
.global StartIdleTask
.global IdleTask
.global GetCtrl
.global SetCtrl
.global CallSWI
.global DisableInterrupts
.global EnableInterrupts
.global SpinLock
.global SpinUnlock
.global InterruptLock
.global InterruptUnlock
.global CopyIn
.global CopyInString
.global CopyOut
.global CopyUserString
.global StrLenUser
.global MemCpy
.global memset
.global FlushCaches
.global InvalidateTLB
.global EnableL1Cache
.global DisableL1Cache
.global MemoryBarrier
.global DataCacheFlush
.global SyncBarrier
.global DataSyncBarrier
.global EnablePaging
.global SetPageDirectory
.global get_current_process
.global get_cpu
.global GetCPSR
.global GetSPSR
.global GetFAR
.global GetDFSR
.global GetIFSR
.global GetDACR
.global SetDACR
.global GetVBAR
.global SetVBAR
    
.global isb
.global dsb
.global dmb


.global svc_stack_top
.global svc_stack
.global interrupt_stack_top
.global interrupt_stack
.global exception_stack_top
.global exception_stack
.global idle_stack_top
.global idle_stack
.global bdflush_stack_top
.global bdflush_stack

.global ifs_stack
.global ifs_stack_top

.global SetContext
.global GetContext


.section .bss

# ****************************************************************************
# Memory reserved for kernel stacks

.balign 4096
svc_stack:
.skip 8092
svc_stack_top:

interrupt_stack:
.skip 8092
interrupt_stack_top:

exception_stack:
.skip 8092
exception_stack_top:

idle_stack:
.skip 4096
idle_stack_top:

bdflush_stack:
.skip 4096
bdflush_stack_top:


# ****************************************************************************

init_stack:
.skip 4096
init_stack_top:


# ****************************************************************************

.section .text

# ****************************************************************************
# Entry point into the kernel,  the bootinfo structure is passed in r0 by
# the bootloader.  Clears bootstrap uninitialized data, saves a pointer to bootinfo
# and calls Main().

_start:
    ldr   sp, =init_stack_top
    ldr   r3, =_ebss
    ldr   r2, =_sbss
    cmp   r2, r3
    bcs   2f
    sub   r3, r2, #1
    ldr   r1, =_ebss
    sub   r1, r1, #1
    mov   r2, #0
1:
    strb   r2, [r3, #1]!
    cmp   r3, r1
    bne   1b

    ldr r4, =bootinfo
    str r0, [r4]
2:


# FIXME: Add or remove this floating point initialization.
#   mrc p15, 0, r0, c1, c0, 2
#   orr r0, r0, #0xC00000            /* double precision */
#   mcr p15, 0, r0, c1, c0, 2
#   mov r0, #0x40000000
#   fmxr fpexc,r0

    msr cpsr_c, #(FIQ_MODE | I_BIT | F_BIT);
    ldr sp, =interrupt_stack_top

    msr cpsr_c, #(IRQ_MODE | I_BIT | F_BIT);
    ldr sp, =interrupt_stack_top

    msr cpsr_c, #(ABT_MODE | I_BIT | F_BIT);
    ldr sp, =exception_stack_top

    msr cpsr_c, #(UND_MODE | I_BIT | F_BIT);
    ldr sp, =exception_stack_top

    msr cpsr_c, #(SYS_MODE | I_BIT | F_BIT);
    ldr sp, =idle_stack_top

    msr cpsr_c, #(SVC_MODE | I_BIT | F_BIT);
    ldr sp, =svc_stack_top

    b Main


# ****************************************************************************
# struct Process *get_current_process();
# struct CPU *GetCPU();

get_current_process:
    MGetCurrentProcess r0
    bx lr


get_cpu:
    ldr r0, =cpu_table
    bx lr


GetCPSR:
    mrs r0, cpsr
    bx lr

GetSPSR:
    mrs r0, spsr
    bx lr

GetFAR:
    mrc p15, 0, r0, c6, c0, 0
    bx lr

GetDFSR:
    mrc p15, 0, r0, c5, c0, 0 
    bx lr

GetIFSR:
    mrc p15, 0, r0, c5, c0, 1 
    bx lr

GetDACR:
    mrc p15, 0, r0, c3, c0, 0;
    bx lr
    
SetDACR:
    mcr p15, 0, r0, c3, c0, 0;
    bx lr

# ****************************************************************************

DisableInterrupts:
    MDisableInterrupts
    bx lr

EnableInterrupts:
    MEnableInterrupts
    bx lr

# TODO - Need to work out variants needed of spinlock functions (interrupt saved?)
SpinLock:
    bx lr

SpinUnlock:
    bx lr

InterruptLock:
    bx lr
    
InterruptUnlock:
    bx lr

# ****************************************************************************
# MemCpy (r0 = dst, r1 = src, r2 = cnt)    r3 = temp

MemCpy:
    cmp r2, #0
    bxeq lr
    ldrb r3, [r1], #1
    strb r3, [r0], #1
    sub r2, #1
    b MemCpy


# ****************************************************************************
# memset (r0 = dst, r1 = val, r2 = sz);

memset:
    cmp r2, #0
    bxeq lr
    strb r1, [r0], #1
    sub r2, #1
    b memset

# ****************************************************************************
# CopyIn

CopyIn:
    push {r0-r1}
    MGetCurrentProcess r0
    ldr r1, =CopyInCatch
    str r1, [r0, #TASK_CATCH_PC]
    pop {r0-r1}
CopyInLoop:
    cmp r2, #0
    beq CopyInDone
    ldrbT r3, [r1], #1
    strb r3, [r0], #1
    sub r2, #1
    b CopyInLoop
CopyInDone:
    MGetCurrentProcess r0
    ldr r1, =#0xdeadbeef
    str r1, [r0, #TASK_CATCH_PC]
    mov r0, #0
    bx lr
CopyInCatch:
    mov r0, #-1
    bx lr

# ****************************************************************************
# CopyInString

CopyInString:
    push {r0-r1}
    MGetCurrentProcess r0
    ldr r1, =CopyInStringCatch
    str r1, [r0, #TASK_CATCH_PC]
    pop {r0-r1}
CopyInStringLoop:
    cmp r2, #0
    beq CopyInStringNoTermDone
    ldrbT r3, [r1], #1
    strb r3, [r0], #1
    cmp r3, #0
    beq CopyInStringDone
    sub r2, #1
    b CopyInStringLoop
CopyInStringDone:
    MGetCurrentProcess r0
    ldr r1, =#0xdeadbeef
    str r1, [r0, #TASK_CATCH_PC]
    mov r0, #0
    bx lr
CopyInStringNoTermDone:
    MGetCurrentProcess r0
    ldr r1, =#0xdeadbeef
    str r1, [r0, #TASK_CATCH_PC]
    mov r0, #-1
    bx lr
CopyInStringCatch:
    mov r0, #-1
    bx lr

# ****************************************************************************
# CopyOut

CopyOut:
    push {r0-r1}
    MGetCurrentProcess r0
    ldr r1, =CopyOutCatch
    str r1, [r0, #TASK_CATCH_PC]
    pop {r0-r1}
CopyOutLoop:
    cmp r2, #0
    beq CopyOutDone
    ldrb r3, [r1], #1
    strbT r3, [r0], #1
    sub r2, #1    
    b CopyOutLoop
CopyOutDone:
    MGetCurrentProcess r0
    ldr r1, =#0xdeadbeef
    str r1, [r0, #TASK_CATCH_PC]
    mov r0, #0
    bx lr
CopyOutCatch:
    mov r0, #-1
    bx lr


# ****************************************************************************
# CopyUserString

CopyUserString:
    push {r0-r1}
    MGetCurrentProcess r0
    ldr r1, =CopyUserStringCatch
    str r1, [r0, #TASK_CATCH_PC]
    pop {r0-r1}
CopyUserStringLoop:
    cmp r2, #0
    beq CopyUserStringNoTermDone
    ldrbT r3, [r1], #1
    strbT r3, [r0], #1
    cmp r3, #0
    beq CopyUserStringDone
    sub r2, #1
    b CopyUserStringLoop
CopyUserStringDone:
    MGetCurrentProcess r0
    ldr r1, =#0xdeadbeef
    str r1, [r0, #TASK_CATCH_PC]
    mov r0, #0
    bx lr
CopyUserStringNoTermDone:
    MGetCurrentProcess r0
    ldr r1, =#0xdeadbeef
    str r1, [r0, #TASK_CATCH_PC]
    mov r0, #-1
    bx lr
CopyUserStringCatch:
    mov r0, #-1
    bx lr


# ****************************************************************************
# len = StrLenUser (str, MAX_LEN);
# r0 = str, r1 = MAX_LEN;

StrLenUser:
    push {r0-r1}
    MGetCurrentProcess r0
    ldr r1, =StrLenUserCatch
    str r1, [r0, #TASK_CATCH_PC]
    pop {r0-r1}

StrLenUserLoop:
    cmp r2, #0
    beq StrLenUserNoTermDone
    ldrbT r3, [r1], #1
//    strbT r3, [r0], #1

    cmp r3, #0
    beq StrLenUserDone
    sub r2, #1
    b StrLenUserLoop

StrLenUserDone:
    MGetCurrentProcess r0
    ldr r1, =#0xdeadbeef
    str r1, [r0, #TASK_CATCH_PC]

// Return length of string, not 0
    mov r0, #0
    bx lr

StrLenUserNoTermDone:
    MGetCurrentProcess r0
    ldr r1, =#0xdeadbeef
    str r1, [r0, #TASK_CATCH_PC]
    mov r0, #-1
    bx lr

StrLenUserCatch:
    mov r0, #-1
    bx lr


# ****************************************************************************

GetTLBType:
    mrc p15, 0, r0, c0, c0, #0
    bx lr
    
SetTLBType:
    mcr p15, 0, r0, c0, c0, #0
    bx lr

GetCtrl:
    mrc p15, 0, r0, c1, c0, #0
    bx lr

SetCtrl:
    mcr p15, 0, r0, c1, c0, #0
    bx lr

GetVBAR:
    mrc p15, 0, r0, c12, c0, #0
    bx lr

SetVBAR:
    mcr p15, 0, r0, c12, c0, #0
    bx lr


# ****************************************************************************

FlushCacheLine:
    mcr p15, 0, r0, c7, c6, #1
    bx lr

MemoryBarrier:
    mov r0, #0
    mcr p15, #0, r0, c7, c10, #5
    bx lr

DataCacheFlush:
    mov r0, #0
    mcr p15, #0, r0, c7, c14, #0
    bx lr

SyncBarrier:
    mov r0, #0
    mcr p15, #0, r0, c7, c10, #4
    bx lr

DataSyncBarrier:
    stmfd sp!, {r0-r8,r12,lr}
    mcr p15, #0, ip, c7, c5, #0
    mcr p15, #0, ip, c7, c5, #6
    mcr p15, #0, ip, c7, c10, #4
    mcr p15, #0, ip, c7, c10, #4
    ldmfd sp!, {r0-r8,r12,pc}
    
InvalidateTLB:
    mov r0,#0
    mcr p15,0,r0,c8,c7,0        // invalidate tlb
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





FlushAllCaches:
		mov	r1, #0
		mcr	p15, 0, r1, c7, c14, 0	@ clean+invalidate D
		mcr	p15, 0, r1, c7, c5, 0	@ invalidate I+BTB
		mcr	p15, 0, r1, c7, c15, 0	@ clean+invalidate unified
		mcr	p15, 0, r1, c7, c10, 4	@ drain WB
		mov	pc, lr

FlushCaches:
    mov r0, #0
    mcr p15, 0, r0, c7, c14, 0;
    mcr p15,0,r0,c7,c7,0        // invalidate caches
    mcr p15,0,r0,c8,c7,0        // invalidate tlb
    bx lr
    
EnableL1Cache:
    mov r0, #0
    mcr p15, 0, r0, c7, c7, 0   // invalidate caches
    mcr p15, 0, r0, c8, c7, 0   // invalidate tlb
    mrc p15, 0, r0, c1, c0, 0
    orr r0,r0,#0x1000           // instruction
    orr r0,r0,#0x0004           // data
    mcr p15, 0, r0, c1, c0, 0
    bx lr

DisableL1Cache:
    mrc p15, 0, r0, c1, c0, 0
    bic r0,r0,#0x1000 ;@ instruction
    bic r0,r0,#0x0004 ;@ data
    mcr p15, 0, r0, c1, c0, 0
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


SetPageDirectory:
    mov r2,#0
    
    // FIXME: Should we be invalidating cache?
//    mcr p15,0,r2,c7,c7,0        // invalidate caches
    mcr p15,0,r2,c8,c7,0        // invalidate tlb
    
    mcr p15,0,r0,c2,c0,0        // tlb base
    mcr p15,0,r0,c2,c0,1        // tlb base
    bx lr






SetContext:
	str	r1, [r0,#4]
	str	r2, [r0,#8]
	str	r3, [r0,#12]
	str	r4, [r0,#16]
	str	r5, [r0,#20]
	str	r6, [r0,#24]
	str	r7, [r0,#28]
	str	r8, [r0,#32]
	str	r9, [r0,#36]
	str	r10, [r0,#40]
	str	r11, [r0,#44]
	str	r12, [r0,#48]
	str	r13, [r0,#52]
	str	r14, [r0,#56]
	/* store 1 as r0-to-restore */
	mov	r1, #1
	str	r1, [r0]
	/* return 0 */
	mov	r0, #0
	mov	pc, lr


GetContext:
	ldr	r1, [r0,#4]
	ldr	r2, [r0,#8]
	ldr	r3, [r0,#12]
	ldr	r4, [r0,#16]
	ldr	r5, [r0,#20]
	ldr	r6, [r0,#24]
	ldr	r7, [r0,#28]
	ldr	r8, [r0,#32]
	ldr	r9, [r0,#36]
	ldr	r10, [r0,#40]
	ldr	r11, [r0,#44]
	ldr	r12, [r0,#48]
	ldr	r13, [r0,#52]
	ldr	r14, [r0,#56]
	ldr	r0, [r0]
	mov	pc, lr

.end


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

.include "kernel/arm/arm.i"
.include "kernel/arm/task.i"
.include "kernel/arm/macros.i"

.global _start
.global GetCtrl
.global SetCtrl
.global CallSWI
.global DisableInterrupts
.global EnableInterrupts
.global DisablePreemption
.global EnablePreemption
.global SpinLock
.global SpinUnlock
.global CopyIn
.global CopyOut
.global MemCpy
.global MemSet
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
.global GetCurrentProcess
.global GetCPU
.global GetCPSR
.global GetSPSR
.global GetFAR
.global GetDFSR
.global GetIFSR
.global StartRootProcess
.global svc_stack_top
.global svc_stack
.global interrupt_stack_top
.global interrupt_stack
.global exception_stack_top
.global exception_stack
.global root_stack_top
.global root_stack
.global idle_task_stack_top
.global idle_task_stack
.global vm_task_stack_top
.global vm_task_stack
.global reaper_task_stack_top
.global reaper_task_stack



.section .data

# ****************************************************************************
# Memory reserved for kernel stacks

.balign 4096
svc_stack:
.skip 4096
svc_stack_top:

.balign 4096
interrupt_stack:
.skip 4096
interrupt_stack_top:

.balign 4096
exception_stack:
.skip 4096
exception_stack_top:

.balign 4096
root_stack:
.skip 4096
root_stack_top:

.balign 2048
idle_task_stack:
.skip 2048
idle_task_stack_top:

.balign 2048
reaper_task_stack:
.skip 2048
reaper_task_stack_top:

.balign 2048
vm_task_stack:
.skip 2048
vm_task_stack_top:



.section .text

# ****************************************************************************
# Entry point into the kernel,  the bootinfo structure is passed in r0 by
# the bootloader.  Clears uninitialized data, saves a pointer to bootinfo
# and calls Main().

_start:
    ldr   r3, =_bss_end
    ldr   r2, =_bss_start
    cmp   r2, r3
    bcs   2f
    sub   r3, r2, #1
    ldr   r1, =_bss_end
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

    msr cpsr_c, #(SVC_MODE | I_BIT | F_BIT);
    ldr sp, =svc_stack_top

    msr cpsr_c, #(SYS_MODE | I_BIT | F_BIT);
    ldr sp, =root_stack_top

    b Main



# ****************************************************************************

StartRootProcess:
    msr cpsr_c, #(SVC_MODE | I_BIT | F_BIT);
    MPrepareStack #CPU_SVC_STACK
    MRestoreUserRegs
    movs pc, lr


# ****************************************************************************
# struct Process *GetCurrentProcess();
# struct CPU *GetCPU();

GetCurrentProcess:
    MGetCurrentProcess r0
    bx lr


GetCPU:
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


# ****************************************************************************

DisableInterrupts:
    MDisableInterrupts
    bx lr

EnableInterrupts:
    MEnableInterrupts
    bx lr

DisablePreemption:
    MDisablePreemption
    bx lr
    
EnablePreemption:
    MEnablePreemption
    bx lr

SpinLock:
    bx lr

SpinUnlock:
    bx lr


# ****************************************************************************
# MemCpy (r0 = dst, r1 = src, r2 = cnt)    r3 = temp

MemCpy:
    cmp r2, #0
    beq 1f
    ldrb r3, [r1], #1
    strb r3, [r0], #1
    sub r2, #1
    b MemCpy
1:
    bx lr


# ****************************************************************************
# MemSet (r0 = dst, r1 = val, r2 = sz);

MemSet:
    cmp r2, #0
    beq 1f
    strb r1, [r0], #1
    sub r2, #1
    b MemSet
1:
    bx lr




CopyIn:
    cmp r2, #0
    beq 1f
    ldrbT r3, [r1], #1
    strb r3, [r0], #1
    sub r2, #1
    b CopyIn
1:
    bx lr


CopyOut:
    cmp r2, #0
    beq 1f
    ldrb r3, [r1], #1
    strbT r3, [r0], #1
    sub r2, #1
    b CopyOut
1:
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


# ****************************************************************************

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
    mcr p15,0,r2,c7,c7,0        // invalidate caches
    mcr p15,0,r2,c8,c7,0        // invalidate tlb
    
    mcr p15,0,r0,c2,c0,0        // tlb base
    mcr p15,0,r0,c2,c0,1        // tlb base
    bx lr














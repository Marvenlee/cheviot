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
 * System call entry points for software interrupts (SWI), exceptions
 * and interrrupts.
 *
 * The cheviot microkernel uses what is known as an interrupt-model kernel.
 * This uses a single kernel stack per CPU as opposed to a kernel stack
 * per thread or process.
 *
 * # In the current ARM code we have only a single CPU but we do have
 * separate stacks for interrupts, exceptions and system calls.  These
 * are allocated within arm.s.
 *
 * What this means is:
 * 1) User-mode state is saved in the process->task_state instead of on
 *    the stack
 * 2) Process can sleep in the kernel, but waking up restarts the
 *    system call. Unless the system call completed and the process
 *    was put to sleep in the __KernelExit part of a call.
 * 
 * There are two locks within the kernel:
 *
 * 1) inkernel_now
 *    This is a spinlock that is locked upon entry to the kernel.
 *    This is locked with the macro MKernelEntryLock. This lock
 *    prevents more than one CPU entering the kernel at the
 *    same time.
 *    
 *    Once a process is in the kernel it can be preempted at any time
 *    by interrupts, in which case the system call starts over when
 *    this process is scheduled again.
 *
 *    Data structures should only be read, data can be copied to and from
 *    user space with CopyIn and CopyOut functions.
 *
 * 2) inkernel_lock
 *    To modify any kernel data structures we need to disable preemption
 *    using DisablePreemption function or the macro equivalent. CopyIn
 *    and CopyOut should not be called.
 *    
 *    Preemption is disabled automatically once we exit the system call's
 *    function written in C and return back to the SWI assembly code. This
 *    is so the program counter and return value can be updated prior
 *    to calling the __KernelExit routine.
 *     
 * In effect a system call is a bit like a transaction, we prepare all
 * the data beforehand with preemption disabled.  Only at the end of 
 * a system call when we want to commit changes do we disable preemption.
 *   
 * __KernelExit code is the common exit path out of the kernel.  This
 * calls KernelExit that expires timers, checks for interrupts and
 * performs the final part of process termination.
 * __KernelExit is also called by code that puts a process to sleep.
 *
 *
 * Further reading on Interrupt-model kernels:
 *  
 *  1) "The 27 Year Old Microkernel" powerpoint presentation On QNX.
 *     This presentation on Neutrino's interrupt-model kernel and how it
 *     achieves kernel preemption is a major influence of the Cheviot
 *     microkernel.
 *  
 *  2) "Interface and Execution Models in the Fluke Kernel" by
 *  Bryan Ford, Mike Hibler, Jay Lepreau, Roland McGrath, Patrick Tullmann
 *  
 *  3) "Single Kernel Stack L4" by Matthew Warton.  This interrupt-model L4
 *     appears to do the opposite of Cheviot and QNX in having interrupts
 *     disabled for the majority of the time.
 */

#include <kernel/arm/arm.i>
#include <kernel/arm/task.i>
#include <kernel/arm/macros.i>

.global reset_vector
.global undef_instr_vector
.global swi_vector
.global prefetch_abort_vector
.global data_abort_vector
.global reserved_vector
.global irq_vector
.global fiq_vector
.global __KernelExit

# ****************************************************************************
# System call table

.balign 16
syscall_table:
    .long UnknownSyscall            // 0
    .long Debug                     // 1
    .long Spawn                     // 2  Executive process only
    .long Exit                      // 3
    .long Join                      // 4
    .long CloseHandle               // 5
    .long WaitFor                   // 6
    .long CheckFor                  // 7
    .long SetSchedParams            // 8
    .long Yield                     // 9
    .long AddInterruptHandler       // 10  
    .long MaskInterrupt             // 11  
    .long UnmaskInterrupt           // 12
    .long CreateTimer               // 13
    .long SetTimer                  // 14
    .long GetSystemTime             // 15
    .long SystemInfo                // 16 
    .long VirtualAlloc              // 17
    .long VirtualAllocPhys          // 18    Privileged, IO processes
    .long VirtualFree               // 19
    .long VirtualProtect            // 20 
    .long VirtualSizeOf             // 21
    .long VirtualVersion            // 22
    .long WireMem                   // 23    Privileged, IO processes
    .long UnwireMem                 // 24    Privileged, IO processes
    .long CreateChannel             // 25
    .long PutMsg                    // 26 
    .long GetMsg                    // 27
    .long PutNotification           // 28
    .long GetNotification           // 29
    .long IsAChannel                // 30
    .long CoalesceVirtualMem        // 31     Privileged, Executive-only
    .long CoalescePhysicalMem       // 32     Privileged, Executive-only
    .long CompactPhysicalMem        // 33     Privileged, Executive-only
    .long PutHandle                 // 34
    .long GetHandle                 // 35
    .long GetNextMsgType            // 36
    
    
#define UNKNOWN_SYSCALL             0
#define MAX_SYSCALL                 36


.balign 16

# ****************************************************************************

reset_vector:
    MPrepareStack #CPU_EXCEPTION_STACK
    bl ResetHandler
    b reset_vector
    
# ****************************************************************************

reserved_vector:
    b reserved_vector

# ****************************************************************************

fiq_vector:
    MPrepareStack #CPU_INTERRUPT_STACK
    bl FiqHandler
    b fiq_vector

# ****************************************************************************

undef_instr_vector:
    MPrepareStack #CPU_EXCEPTION_STACK
    
    MTestSPSRMode #SVC_MODE
    beq 1f
        
    MSaveUserRegs
    MKernelEntryLock
    
1:
    bl UndefInstrHandler
    msr cpsr_c, #(SVC_MODE | I_BIT | F_BIT);
    b __KernelExit
    

# ****************************************************************************

prefetch_abort_vector:
    MPrepareStack #CPU_EXCEPTION_STACK
    sub lr, #4
    
    MTestSPSRMode #SVC_MODE
    beq 1f
        
    MSaveUserRegs
    MKernelEntryLock
    
1:
    bl PrefetchAbortHandler
    msr cpsr_c, #(SVC_MODE | I_BIT | F_BIT);
    b __KernelExit
    
# ****************************************************************************

data_abort_vector:
    MPrepareStack #CPU_EXCEPTION_STACK
    sub lr, #8
    
    MTestSPSRMode #SVC_MODE
    beq 1f
        
    MSaveUserRegs
    MKernelEntryLock
    
1:
    bl DataAbortHandler
    msr cpsr_c, #(SVC_MODE | I_BIT | F_BIT);
    b __KernelExit
    



# ****************************************************************************

irq_vector:
    MPrepareStack #CPU_INTERRUPT_STACK
    sub lr, #4
    
    MTestSPSRMode #SVC_MODE
    beq 2f

    MSaveUserRegs
    MKernelEntryLock

1:
    bl InterruptTopHalf
    msr cpsr, #(SVC_MODE | I_BIT | F_BIT)
    b __KernelExit

2:
    MTestPreemptionLock #1
    beq 3f

    bl InterruptTopHalf
    msr cpsr, #(SVC_MODE | I_BIT | F_BIT)
    b __KernelExit

3:  
    push {r0-r12, lr}
    mrs r0, spsr
    push {r0}
    
    bl InterruptTopHalf
    pop {r0}
    msr spsr, r0
    pop {r0-r12, lr}
    movs pc, lr

    







# ****************************************************************************

swi_vector:
    MPrepareStack #CPU_SVC_STACK
    sub lr, #4                      // Subtract #4 to restart instruction
                                    // Assumes no thumb code.
    
    MSaveUserRegs

    MKernelEntryLock

    MEnableInterrupts

    ldr r7, [lr]                    // Get syscall nr and check range
    bic r7,r7, #0xff000000
    cmp r7, #MAX_SYSCALL
    movgt r7, #UNKNOWN_SYSCALL
        
    MGetCurrentProcess r6           // Reload syscall arguments into r0-r5
    ldr r0, [r6, #TASK_R0]
    ldr r1, [r6, #TASK_R1]
    ldr r2, [r6, #TASK_R2]
    ldr r3, [r6, #TASK_R3]
    ldr r4, [r6, #TASK_R4]
    ldr r5, [r6, #TASK_R5]
    push {r4-r5}                    // Store arguments r4-r5 on stack
    ldr r4, =syscall_table
    add r4, r7, LSL #2
    ldr r7, [r4]
    blx r7
    add sp, #8                      // Remove arg 5 and 6 from stack
    
    mov r7, r0
    MGetCurrentProcess r6
    MDisablePreemption
    str r7, [r6, #TASK_R0]
    ldr r0, [r6, #TASK_PC]
    add r0, #4
    str r0, [r6, #TASK_PC]          // Control goes straight to __kernelexit


        
__KernelExit:
    MPrepareStack #CPU_SVC_STACK    // At this point preemption state
                                    // may be either enabled or disabled.
    MEnableInterrupts               // Interrupts are enabled.
    bl KernelExit                   // __KernelExit can be called from
    MDisableInterrupts              // many places, including Sleep()
    MKernelEntryUnlock              // and exception handlers
    MRestoreUserRegs
    movs pc, lr







.end











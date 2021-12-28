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

#include <kernel/arm/arm.i>
#include <kernel/arm/task.i>
#include <kernel/arm/macros.i>

.global swi_vector
.global prefetch_abort_vector
.global data_abort_vector
.global reserved_vector
.global irq_vector
.global reset_vector
.global undef_instr_vector
.global fiq_vector
.global StartForkProcess
.global StartExecProcess
.global StartKernelProcess

# *************************************************************************
# System call table

.section .text

.balign 16
syscall_table:
    .long SysUnknownSyscallHandler     // 0
    .long SysDebug                     // 1    TODO: Eventually remove

    .long SysFork	                     // 2  
    .long SysExit                      // 3
    .long SysWaitPid                   // 4
    .long SysKill                      // 5
    .long SysSetSchedParams            // 6 

    .long SysSleep                     // 7   
    .long SysAlarm                     // 8
    .long 0xfee1501d                   // 9    TODO: Remove, or add a createtimer syscall
    .long SysGetTimeOfDay              // 10
    .long SysSetTimeOfDay              // 11
    
    .long SysVirtualAlloc              // 12   TODO: Merge into mmap or variant
    .long SysVirtualAllocPhys          // 13   TODO: Merge with above
    .long SysVirtualFree               // 14
    .long SysVirtualProtect            // 15 

    .long SysCreateInterrupt           // 16
    .long SysMaskInterrupt             // 17
    .long SysUnmaskInterrupt           // 18

    .long SysPoll                      // 19   TODO: Replace with select + epoll-like event API
        
    .long SysExec                      // 20   
     
    .long SysMount                     // 21 
    .long SysUnmount                   // 22

    // Replace with just Read/Write calls. No seek.  Write header then immediately the data.
    .long SysSeekMsg                   // 23  FIXME: Replace with read/write
    .long SysReceiveMsg                // 24  FIXME: Replace with read/write
    .long SysReplyMsg                  // 25  FIXME: Replace with read/write
    .long SysReadMsg                   // 26  FIXME: Replace with read/write
    .long SysWriteMsg                  // 27  FIXME: Replace with read/write
        
    .long SysOpen                      // 28   
    .long SysClose                     // 29
    .long SysDup                       // 30
    .long SysDup2                      // 31
    
    .long SysRead                      // 32
    .long SysWrite                     // 33
    .long SysSeek                      // 34  FIXME: Why 2 syscalls for Seek ?
    .long SysSeek64                    // 35
    
    .long SysTruncate                  // 36
    .long SysUnlink                    // 37

    .long SysCreateDir                 // 38
    .long SysOpenDir                   // 39
    .long SysReadDir                   // 40
    .long SysRewindDir                 // 41
    .long SysRemoveDir                 // 42

    .long SysRename                    // 43

    .long SysPipe                      // 44 Pipe
    .long 0xdeadcaf3                // 45 SocketPair

    .long SysChDir                     // 46
    .long SysFChDir                    // 47       
    .long SysStat                      // 48
    .long SysFStat                     // 49

    .long SysSymLink                   // 50
    .long SysReadLink                  // 51

    .long SysChmod                     // 52
    .long SysChown                     // 53
    .long SysAccess                    // 54
    .long SysUmask                     // 55

    .long SysGetPID                    // 56
    .long SysGetPPID                   // 57
    .long SysGetUID                    // 58
    .long SysGetGID                    // 59
    .long SysGetEUID                   // 60
    .long SysGetEGID                   // 61
    
    .long SysSetUID                    // 62
    .long SysSetGID                    // 63
    .long SysSetPGRP                   // 64    
    .long SysGetPGRP                   // 65
    
    .long SysVirtualToPhysAddr         // 66  // FIXME: Remove, not needed
    
    .long SysSignalNotify              // 67  // FIXME: Remove, (Replace with write() call when ***Msg calls removed)
    .long SysPollNotify                // 68  // FIXME: Remove, (Replace with write() call when ***Msg calls removed)

    .long SysPivotRoot                 // 69

    .long SysFcntl                     // 70  
    .long SysIsATTY                    // 71  // FIXME: Could be SendRec/Ioctl
    .long SysIoctl                     // 72  // Could be generic SendRec(fd, iov_send[], iov_recv[]);
    
    .long SysSync                      // 73   // Could merge with FSync
    .long SysFSync                     // 74

    .long SysSigAction                 // 75
    .long SysSigProcMask               // 76
    .long SysSigPending                // 77
    .long SysSigSuspend                // 78
    
    .long SysMkNod                     // 79
    .long SysMoveMount                 // 80
    
#define UNKNOWN_SYSCALL             0
#define MAX_SYSCALL                 80


# ****************************************************************************

.balign 16

swi_vector:
    MSWIPushRegs
    MEnableInterrupts
    
    bl KernelLock

    ldr lr, [sp, #CONTEXT_PC]
    ldr r7, [lr, #-4]                   // Get syscall nr and check range  (possible to fault, hence getting lock beforehand
    bic r7,r7, #0xff000000              // Eventually handle thumb mode.
    cmp r7, #MAX_SYSCALL
    movgt r7, #UNKNOWN_SYSCALL

    ldr r0, [sp, #CONTEXT_R0]           // Surely we can avoid touching all 4 args registers?
    ldr r1, [sp, #CONTEXT_R1]
    ldr r2, [sp, #CONTEXT_R2]
    ldr r3, [sp, #CONTEXT_R3]
    
    ldr r4, [sp, #CONTEXT_R4]           // ?? Really needed? or max 4 orgs?
    ldr r5, [sp, #CONTEXT_R5]
    sub sp, #8    
    str r4, [sp, #0]          // Alignment (for 64-bit values?)
    str r5, [sp, #4]          // Store arguments r4-r5 on stack    ?
    ldr r4, =syscall_table
    add r4, r7, LSL #2
    ldr r7, [r4]
    blx r7
    add sp, #8
    str r0, [sp, #CONTEXT_R0]    
    mov r0, sp
    bl CheckSignals
       
    bl KernelUnlock    
    MDisableInterrupts        
    MSWIPopRegs
    subs pc, lr, #0

# ****************************************************************************

.balign 16

irq_vector:
    sub lr, #4
    MPushRegs    

    mov r0, sp    
    bl InterruptHandler

    mov r0, sp
    bl CheckSignals
    
    MPopRegs    
    subs pc, lr, #0




# ****************************************************************************

.balign 16

undef_instr_vector:
    MPushRegs
    
    mov r0, sp    
    bl UndefInstrHandler

    mov r0, sp
    bl CheckSignals
    
    MPopRegs    
    subs pc, lr, #0
    

# ****************************************************************************

.balign 16

prefetch_abort_vector:
    sub lr, #4
    MPushRegs   

    mov r0, sp    
    bl PrefetchAbortHandler

    mov r0, sp
    bl CheckSignals

    MPopRegs    
    subs pc, lr, #0

    
# ****************************************************************************

.balign 16

data_abort_vector:
    sub lr, #8
    MPushRegs

    mov r0, sp
    bl DataAbortHandler

    mov r0, sp
    bl CheckSignals
    
    MPopRegs    
    subs pc, lr, #0
        

# ****************************************************************************

.balign 16

reset_vector:
    bl ResetHandler
    bl reset_vector
    
reserved_vector:
    bl ReservedHandler    
    bl reserved_vector

fiq_vector:
    bl FiqHandler
    bl fiq_vector


# ****************************************************************************

StartForkProcess:
    bl KernelLock    
    mov r0, sp
    bl CheckSignals
    bl KernelUnlock
     
    MDisableInterrupts
    MSWIPopRegs
    subs pc, lr, #0


StartExecProcess:
    mov r0, sp    
    bl CheckSignals
    bl KernelUnlock
     
    MDisableInterrupts
    MSWIPopRegs
    subs pc, lr, #0


StartKernelProcess:
    push {r0}
    bl KernelLock
    pop {r0}
    blx r0
    mov r0, #0
    bl SysExit    
    

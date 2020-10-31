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
    .long UnknownSyscallHandler     // 0
    .long Debug                     // 1    TODO: Eventually remove

    .long Fork	                    // 2  
    .long Exit                      // 3
    .long WaitPid                   // 4
    .long Kill                      // 5
    .long SetSchedParams            // 6 

    .long Sleep                     // 7   
    .long Alarm                     // 8
    .long 0xfee1501d                // 9    TODO: Remove
    .long GetTimeOfDay              // 10
    .long SetTimeOfDay              // 11
    
    .long VirtualAlloc             	// 12   TODO: Merge into mmap or variant
    .long VirtualAllocPhys          // 13   TODO: Merge with above
    .long VirtualFree              	// 14
    .long VirtualProtect            // 15 

    .long CreateInterrupt           // 16
    .long MaskInterrupt             // 17
    .long UnmaskInterrupt           // 18

    .long Poll                      // 19   TODO: Replace with select + epoll-like event API
        
    .long Exec                      // 20
     
    .long Mount                     // 21 
    .long Unmount                   // 22

    // Replace with just Read/Write calls. No seek.  Write header then immediately the data.
    .long SeekMsg                   // 23  FIXME: Replace with read/write
    .long ReceiveMsg                // 24  FIXME: Replace with read/write
    .long ReplyMsg                  // 25  FIXME: Replace with read/write
    .long ReadMsg                   // 26  FIXME: Replace with read/write
    .long WriteMsg                  // 27  FIXME: Replace with read/write
        
    .long Open                      // 28   
    .long Close                     // 29
    .long Dup                       // 30
    .long Dup2                      // 31
    
    .long Read                      // 32
    .long Write                     // 33
    .long Seek                      // 34  FIXME: Why 2 syscalls for Seek ?
    .long Seek64                    // 35
    
    .long Truncate                  // 36
    .long Unlink                    // 37

    .long CreateDir                 // 38
    .long OpenDir                   // 39
    .long ReadDir                   // 40
    .long RewindDir                 // 41
    .long RemoveDir                 // 42

    .long Rename                    // 43

    .long Pipe                      // 44 Pipe
    .long 0xdeadcaf3                // 45 SocketPair

    .long ChDir                     // 46
    .long FChDir                    // 47       
    .long Stat                      // 48
    .long FStat                     // 49

    .long SymLink                   // 50
    .long ReadLink                  // 51

    .long Chmod                     // 52
    .long Chown                     // 53
    .long Access                    // 54
    .long Umask                     // 55

    .long GetPID                    // 56
    .long GetPPID                   // 57
    .long GetUID                    // 58
    .long GetGID                    // 59
    .long GetEUID                   // 60
    .long GetEGID                   // 61
    
    .long SetUID                    // 62
    .long SetGID                    // 63
    .long SetPGRP                   // 64    
    .long GetPGRP                   // 65
    
    .long VirtualToPhysAddr         // 66  // FIXME: Remove, not needed
    
    .long SignalNotify              // 67  // FIXME: Remove, (Replace with write() call when ***Msg calls removed)
    .long PollNotify                // 68  // FIXME: Remove, (Replace with write() call when ***Msg calls removed)

    .long PivotRoot                 // 69

    .long Fcntl                     // 70  
    .long IsATTY                    // 71  // FIXME: Could be SendRec/Ioctl
    .long Ioctl                     // 72  // Could be generic SendRec(fd, iov_send[], iov_recv[]);
    
    .long Sync                      // 73   // Could merge with FSync
    .long FSync                     // 74

    .long SigAction                 // 75
    .long SigProcMask               // 76
    .long SigPending                // 77
    .long SigSuspend                // 78
    
    .long MkNod                     // 79
    .long Remount                   // 80
    
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
    bl Exit    
    

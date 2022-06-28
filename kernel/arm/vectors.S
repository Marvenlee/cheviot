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
    .long sys_unknownsyscallhandler     // 0
    .long sys_debug                     // 1    TODO: Eventually remove

    .long sys_fork	                    // 2  
    .long sys_exit                      // 3
    .long sys_waitpid                   // 4
    .long sys_kill                      // 5
    .long sys_setschedparams            // 6 

    .long sys_sleep                     // 7   
    .long sys_alarm                     // 8
    .long 0xfee1501d                    // 9    TODO: Remove, or add a createtimer syscall
    .long sys_gettimeofday              // 10
    .long sys_settimeofday              // 11
    
    .long sys_virtualalloc              // 12   TODO: Merge into mmap or variant
    .long sys_virtualallocphys          // 13   TODO: Merge with above
    .long sys_virtualfree               // 14
    .long sys_virtualprotect            // 15 

    .long sys_createinterrupt           // 16
    .long sys_maskinterrupt             // 17
    .long sys_unmaskinterrupt           // 18

    .long sys_poll                      // 19   TODO: Replace with select + epoll-like event API
        
    .long sys_exec                      // 20   
     
    .long sys_mount                     // 21 
    .long sys_unmount                   // 22

    // Replace with just Read/Write calls. No seek.  Write header then immediately the data.
    // Need listen(mount_fd) to create new listener handle (multiple for char device server)
    
    .long sys_seekmsg                   // 23  FIXME: Replace with read/write
    .long sys_receivemsg                // 24  FIXME: Replace with read/write
    .long sys_replymsg                  // 25  FIXME: Replace with read/write
    .long sys_readmsg                   // 26  FIXME: Replace with read/write
    .long sys_writemsg                  // 27  FIXME: Replace with read/write
        
    .long sys_open                      // 28   
    .long sys_close                     // 29
    .long sys_dup                       // 30
    .long sys_dup2                      // 31
    
    .long sys_read                      // 32
    .long sys_write                     // 33
    .long sys_lseek                     // 34  FIXME: Why 2 syscalls for Seek ?
    .long sys_lseek64                   // 35
    
    .long sys_truncate                  // 36
    .long sys_unlink                    // 37

    .long sys_createdir                 // 38
    .long sys_opendir                   // 39
    .long sys_readdir                   // 40
    .long sys_rewinddir                 // 41
    .long sys_rmdir                     // 42

    .long sys_rename                    // 43

    .long sys_pipe                      // 44 Pipe
    .long 0xdeadcaf3                    // 45 SocketPair

    .long sys_chdir                     // 46
    .long sys_fchdir                    // 47       

    .long sys_stat                      // 48
    .long sys_fstat                     // 49

    .long sys_symlink                   // 50
    .long sys_readlink                  // 51

    .long sys_chmod                     // 52
    .long sys_chown                     // 53
    .long sys_access                    // 54
    .long sys_umask                     // 55

    .long sys_getpid                    // 56
    .long sys_getppid                   // 57
    .long sys_getuid                    // 58
    .long sys_getgid                    // 59
    .long sys_geteuid                   // 60
    .long sys_getegid                   // 61
    
    .long sys_setuid                    // 62
    .long sys_setgid                    // 63
    .long sys_setpgrp                   // 64    
    .long sys_getpgrp                   // 65
    
    .long sys_virtualtophysaddr          // 66  // FIXME: Remove, not needed
    
    .long sys_signalnotify              // 67  // FIXME: Remove, (Replace with write() call when ***Msg calls removed)
    .long sys_pollnotify                // 68  // FIXME: Remove, (Replace with write() call when ***Msg calls removed)

    .long sys_pivotroot                 // 69

    .long sys_fcntl                     // 70  
    .long sys_isatty                    // 71  // FIXME: Could be SendRec/Ioctl
    .long sys_ioctl                     // 72  // Could be generic SendRec(fd, iov_send[], iov_recv[]);
    
    .long sys_sync                      // 73   // Could merge with FSync
    .long sys_fsync                     // 74

    .long sys_sigaction                 // 75
    .long sys_sigprocmask               // 76
    .long sys_sigpending                // 77
    .long sys_sigsuspend                // 78
    
    .long sys_mknod                     // 79
    .long sys_movemount                 // 80
    
    .long sys_chroot                    // 81
    
#define UNKNOWN_SYSCALL             0
#define MAX_SYSCALL                 81


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
    bl sys_exit    
    

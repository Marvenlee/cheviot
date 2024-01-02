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

#include <kernel/board/arm.i>
#include <kernel/board/task.i>
#include <kernel/board/macros.i>

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

.section .text


// @brief    System call table
//
// System call table of the kernel functions exposed to user mode through the swi
// trap / software interrupt mechanism.
// 
// The call flow from app to kernel function is:
//
// app -> (libc) write -> _swi_write -> (kernel mode) swi_vector -> sys_write -> ...
//
// Newlib is used as the C standard Library. See the following newlib file:
// third_party/newlib-4.1.0/newlib/libc/sys/arm/syscall.S for the corresponding
// _swi_xxxx functions.
//
.balign 16
syscall_table:
    .long sys_unknownsyscallhandler     // 0
    .long sys_debug                     // 1

    .long sys_fork	                    // 2  
    .long sys_exit                      // 3
    .long sys_waitpid                   // 4
    .long sys_kill                      // 5
    .long sys_setschedparams            // 6 

    .long sys_sleep                     // 7   
    .long sys_alarm                     // 8
    .long 0xfee1501d                    // 9    TODO: Add a createtimer syscall
    .long sys_gettimeofday              // 10
    .long sys_settimeofday              // 11
    
    .long sys_virtualalloc              // 12   TODO: Replace with mmap
    .long sys_virtualallocphys          // 13   TODO: Replace with mmap
    .long sys_virtualfree               // 14   TODO: Replace with munmap
    .long sys_virtualprotect            // 15   TODO: Replace with mprotect

    .long sys_createinterrupt           // 16
    .long sys_maskinterrupt             // 17
    .long sys_unmaskinterrupt           // 18

    .long 0xdeadbaaf                    // 19
        
    .long sys_exec                      // 20   
     
    .long sys_mount                     // 21 
    .long 0xdeadbeaf                    // 22  TODO: Was sys_unmount

    // Replace with just Read/Write calls. No seek.  Write header then immediately the data.
    // Need listen(mount_fd) to create new listener handle (multiple for char device server)
    
    .long sys_sendrec                   // 23
    .long sys_getmsg                    // 24  
    .long sys_replymsg                  // 25
    .long sys_readmsg                   // 26
    .long sys_writemsg                  // 27
        
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
    
    .long sys_virtualtophysaddr         // 66
    
    .long sys_signalnotify              // 67 
    .long sys_knotei                    // 68
    
    .long sys_pivotroot                 // 69

    .long sys_fcntl                     // 70  
    .long sys_isatty                    // 71  
    .long sys_ioctl                     // 72    
    .long sys_sync                      // 73
    .long sys_fsync                     // 74

    .long sys_sigaction                 // 75
    .long sys_sigprocmask               // 76
    .long sys_sigpending                // 77
    .long sys_sigsuspend                // 78
    
    .long sys_mknod                     // 79
    .long sys_movemount                 // 80
    
    .long sys_chroot                    // 81

    .long sys_kqueue                    // 82 
    .long sys_kevent                    // 83

    // .long sys_sigreturn

    /*  
    .long sys_thread_create
    .long sys_thread_exit
    .long sys_thread_join
    .long sys_thread_cancel
    .long sys_thread_setprio
    .long sys_thread_setschedparam    
    .long sys_thread_self
    
    .long sys_pthread_mutex_lock
    .long sys_pthread_mutex_unlock

    .long sys_pthread_cond_wait
    .long sys_pthread_cond_timedwait
    .long sys_pthread_cond_signal
    .long sys_pthread_cond_broadcast

    .long sys_thread_rwlock_rdlock
    .long sys_thread_rwlock_unlock
    .long sys_thread_rwlock_wrlock
    */

    
#define UNKNOWN_SYSCALL             0
#define MAX_SYSCALL                 83


// @brief   System call table that can be called from interrupt handlers
//
// TODO: Currently interrupt handlers registered with CreateInterrupt run
// in kernel mode and use a struct InterruptAPI with function pointers to
// the below functions.  Replace it with interrupts running in user-mode
// with interrupts disabled and only allow the following 3 syscalls.
// Perhaps add exit_From_isr to abort process.

.balign 16
isr_syscall_table:
  .long MaskInterruptFromISR        // 0
  .long UnmaskInterruptFromISR      // 1
  .long knote_from_isr              // 2

#define MAX_ISR_SYSCALL             2


// @brief   System call entry point
//
// System calls can take up to 6 parameters in registers R0 to R5.
// This assembly code saves the register state on the stack before acquiring
// the Big Kernel Lock (BKL). After the lock is acquired
// the corresponding sys_xxxx function is called.
//
// Any task operating within the kernel is scheduled co-operatively (coroutining).
// A task switch only occurs if a task blocks, for example due to termination,
// sleeping or waiting on a condition variable with TaskSleep() in proc/sched.c
//
// This is similar to early versions of UNIX-like systems. The Big Kernel Lock
// acts as the surrounding mutex in a monitor with TaskSleep() and TaskWakeup()
// operating on a monitor's condition variables.
//
// The plan will be to make the kernel pre-emptive at a later date.
//
// Upon system call completion this checks if there is any pending signals
// and pops the registers from the stack.  R0 and R1 are used to return
// the result of the system call to user space.
//
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

    ldr r0, [sp, #CONTEXT_R0]
    ldr r1, [sp, #CONTEXT_R1]
    ldr r2, [sp, #CONTEXT_R2]
    ldr r3, [sp, #CONTEXT_R3]
    
    ldr r5, [sp, #CONTEXT_R5] // Read R4 and R5 from the context
    ldr r4, [sp, #CONTEXT_R4]
    sub sp, #8                // Why not use "push" instruction to push registers on stack ? 
    str r4, [sp, #0]          // Store R4 and R5 on stack, these are the 5th and 6th arguments of a syscall.
    str r5, [sp, #4]          
    ldr r4, =syscall_table
    add r4, r7, LSL #2
    ldr r7, [r4]
    blx r7
    add sp, #8                // Remove R4 and R5 5th and 6th arguments from stack
    str r0, [sp, #CONTEXT_R0]    
    mov r0, sp
    bl CheckSignals
       
    bl KernelUnlock    
    MDisableInterrupts        
    MSWIPopRegs
    subs pc, lr, #0


// @brief   Entry point for interrupts
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


// @brief   Entry point for undefined instruction exceptions
.balign 16

undef_instr_vector:
    MPushRegs
    
    mov r0, sp    
    bl UndefInstrHandler

    mov r0, sp
    bl CheckSignals
    
    MPopRegs    
    subs pc, lr, #0
    

// @brief   Entry point into kernel for instruction prefetch exceptions
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

    
// @brief   Entry point for exceptions caused by failed reads or writes to memory
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
    

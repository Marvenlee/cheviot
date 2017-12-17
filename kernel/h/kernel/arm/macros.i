//.include "kernel/arm/arm.i"
//.include "kernel/arm/task.i"


// Uniprocessor Locks

.macro MPrepareStack stack
    ldr sp, = cpu_table
    ldr sp, [sp, \stack]
.endm

.macro MKernelEntryLock
    push {r0-r1}
    ldr r0, = inkernel_now
    mov r1, #1
    str r1, [r0]
    pop {r0-r1}
.endm


.macro MKernelEntryUnlock
    push {r0-r1}
    ldr r0, = inkernel_now
    mov r1, #0
    str r1, [r0]
    pop {r0-r1}
.endm



.macro MDisablePreemption
    push {r0-r1}
    ldr r0, = inkernel_lock
    mov r1, #1
    str r1, [r0]
    pop {r0-r1}
.endm


.macro MEnablePreemption
    push {r0-r2}
    ldr r1, =cpu_table
    ldr r2, [r1, #CPU_RESCHEDULE_REQUEST]
    ldr r0, = inkernel_lock
    mov r1, #0
    str r1, [r0]
    cmp r2, #0
    pop {r0-r2}
    bne __KernelExit
.endm

.macro MTestPreemptionLock val
    push {r0}
    ldr r0, = inkernel_lock
    ldr r0, [r0]
    cmp r0, \val
    pop {r0}
.endm


.macro MTestSPSRMode val
    push {r0}
    mrs r0, spsr
    and r0, #MODE_MASK
    cmp r0, \val
    pop {r0}
.endm

.macro MEnableInterrupts
    push {r0}   
    mrs r0, cpsr        
    bic r0, r0, #0x80
    msr cpsr_c, r0
    pop {r0}
.endm


.macro MDisableInterrupts
    push {r0}
    mrs r0, cpsr
    orr r0, r0, #0x80
    msr cpsr_c, r0
    pop {r0}
.endm




.macro MGetCurrentProcess reg
    ldr \reg, =cpu_table
    ldr \reg, [\reg, #CPU_CURRENT_PROCESS]

.endm





.macro MSaveUserRegs
    push {lr}
    push {r0}
    mrs r0, spsr
    push {r0}
    MGetCurrentProcess r0
    add r0, #12
    stm r0, {r1-r14}^
    sub r0, #12
    pop {r1-r3}
    stm r0, {r1 - r3}
.endm


.macro MRestoreUserRegs
    MGetCurrentProcess r0
    ldm r0, {r1 - r3}
    push {r1 - r3}
    add r0, #12
    ldm r0, {r1-r14}^
    pop {r0}
    msr spsr, r0
    pop {r0}
    pop {lr}
.endm   
    


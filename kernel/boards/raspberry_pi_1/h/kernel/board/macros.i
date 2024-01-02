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


/* @brief   Enable interrupts on current processor
 * TODO: Removed old code that didn't restore interrupt state
 */
//.macro MEnableInterrupts
//    push {r0}   
//    mrs r0, cpsr        
//    bic r0, r0, #0x80
//    msr cpsr_c, r0
//    pop {r0}
//.endm

/* @brief   Disable interrupts on current processor
 *
 */
//.macro MDisableInterrupts
//    push {r0}
//    mrs r0, cpsr
//    orr r0, r0, #0x80
//    msr cpsr_c, r0
//    pop {r0}
//.endm

/* @brief   Enable interrupts on current processor
 *
 * cpsie i
 */
.macro MEnableInterrupts
//    mrs r0, cpsr
//    orr r0, r0, #0x80     // This seems wrong, it is clear to enable interrupts, set bit to mask them

    cpsie i             // Enable only normal interrupts, not fiq
//    msr cpsr_c, r0
.endm

/* @brief   Disable interrupts on current processor and return previous state
 *
 */
.macro MDisableInterrupts
    mrs r0, cpsr        
    cpsid fi                  
    and r0, r0, #0x80   // previously and. // FIXME: interrupt mask was 0xC0 here int bit and cpu mode irq bit
.endm


/* @brief   Restore interrupt state on current processor
 *
 */
.macro MRestoreInterrupts
    and r0, r0, #0x80 
    
    mrs r1, cpsr
//    bic r1, r1, r0

    orr r1, r1, r0
    msr cpsr_c, r1  
.endm



/* @brief   Get a pointer to the current process
 *
 */
.macro MGetCurrentProcess reg
    ldr \reg, =cpu_table
    ldr \reg, [\reg, #CPU_CURRENT_PROCESS]
.endm


/* @brief   Store register context in a UserContext structure from a SWI interrupt
 *
 */
.macro MSWIPushRegs
    sub sp, #SIZEOF_CONTEXT
    str lr, [sp,#CONTEXT_PC]
    str r0, [sp,#CONTEXT_R0]
  	str	r1, [sp,#CONTEXT_R1]
  	str	r2, [sp,#CONTEXT_R2]	
  	str	r3, [sp,#CONTEXT_R3]
  	str	r4, [sp,#CONTEXT_R4]
  	str	r5, [sp,#CONTEXT_R5]
  	str	r6, [sp,#CONTEXT_R6]
  	str	r7, [sp,#CONTEXT_R7]
  	str	r8, [sp,#CONTEXT_R8]
  	str	r9, [sp,#CONTEXT_R9]
  	str	r10, [sp,#CONTEXT_R10]
  	str	r11, [sp,#CONTEXT_R11]
  	str	r12, [sp,#CONTEXT_R12]
    mrs r0, spsr
  	str	r0, [sp,#CONTEXT_CPSR]

    mrs r3, cpsr

    msr cpsr, #(SYS_MODE | I_BIT | F_BIT)
    mov r0, sp
    mov r1, lr
    msr cpsr, r3
    str r0, [sp,#CONTEXT_SP]
    str r1, [sp,#CONTEXT_LR]
.endm


/* @brief   Reload user registers from a UserContext structure when returning from a SWI interrupt
 *
 */
.macro MSWIPopRegs
    ldr r0, [sp,#CONTEXT_SP]
    ldr r1, [sp,#CONTEXT_LR]

    mrs r3, cpsr
    ldr r2, [sp, #CONTEXT_CPSR]
    mov r2, #SYS_MODE
    orr r2, #(I_BIT | F_BIT)
    msr cpsr_c, r2

    mov sp, r0
    mov lr, r1

    msr cpsr,r3                 
	
	  ldr r0, [sp,#CONTEXT_CPSR]
    msr spsr, r0    
    ldr lr, [sp,#CONTEXT_PC]
    ldr r0, [sp,#CONTEXT_R0]
	  ldr r1, [sp,#CONTEXT_R1]
  	ldr r2, [sp,#CONTEXT_R2]	
  	ldr r3, [sp,#CONTEXT_R3]
	  ldr r4, [sp,#CONTEXT_R4]
  	ldr r5, [sp,#CONTEXT_R5]
  	ldr r6, [sp,#CONTEXT_R6]
	  ldr r7, [sp,#CONTEXT_R7]
	  ldr r8, [sp,#CONTEXT_R8]
	  ldr r9, [sp,#CONTEXT_R9]
	  ldr r10, [sp,#CONTEXT_R10]
	  ldr r11, [sp,#CONTEXT_R11]
	  ldr r12, [sp,#CONTEXT_R12]
    add sp, #SIZEOF_CONTEXT
    
.endm


/* @brief   Store registers in a UserContext structure from an exception  
 *
 * Note that exceptions can occur in user or kernel mode
 */
.macro MPushRegs
    sub sp, #SIZEOF_ALT
    str lr, [sp, #ALT_PC]
    str r0, [sp, #ALT_R0]
    str r1, [sp, #ALT_R1]
    str r2, [sp, #ALT_R2]
    mrs r1, spsr
    str r1, [sp, #ALT_CPSR]
    
    mov r0, sp
    add sp, #SIZEOF_ALT

    mov r2, #SVC_MODE    
    and r1, #MODE_MASK
    cmp r1, #USR_MODE
    moveq r2, #SYS_MODE
    orr r2, #(I_BIT | F_BIT)
    msr cpsr_c, r2
    mov r1, sp
    mov r2, lr    
    msr cpsr_c, #(SVC_MODE | I_BIT | F_BIT)
    
    sub sp, #SIZEOF_CONTEXT
  	str	r1, [sp,#CONTEXT_SP]
  	str	r2, [sp,#CONTEXT_LR]
  	str	r3, [sp,#CONTEXT_R3]
  	str	r4, [sp,#CONTEXT_R4]
  	str	r5, [sp,#CONTEXT_R5]
  	str	r6, [sp,#CONTEXT_R6]
  	str	r7, [sp,#CONTEXT_R7]
  	str	r8, [sp,#CONTEXT_R8]
  	str	r9, [sp,#CONTEXT_R9]
  	str	r10, [sp,#CONTEXT_R10]
  	str	r11, [sp,#CONTEXT_R11]
  	str	r12, [sp,#CONTEXT_R12]

    ldr r1, [r0, #ALT_CPSR]
    ldr r2, [r0, #ALT_R0]
    ldr r3, [r0, #ALT_R1]
    ldr r4, [r0, #ALT_R2]           
    ldr r5, [r0, #ALT_PC]
    str r1, [sp,#CONTEXT_CPSR]
    str r2, [sp,#CONTEXT_R0]
    str r3, [sp,#CONTEXT_R1]
    str r4, [sp,#CONTEXT_R2]
    str r5, [sp,#CONTEXT_PC]
.endm


/* @brief   Reload registers from a UserContext structure when returning from an exception  
 *
 * Note that exceptions can occur in user or kernel mode
 */
.macro MPopRegs
    mov r0, sp
    ldr r1, [sp,#CONTEXT_CPSR]
    ldr r2, [sp,#CONTEXT_R0]
    ldr r3, [sp,#CONTEXT_R1]
    ldr r4, [sp,#CONTEXT_R2]
    ldr r5, [sp,#CONTEXT_PC]

    ldr r6, [sp,#CONTEXT_SP]
    ldr r7, [sp,#CONTEXT_LR]
    
    add sp, #SIZEOF_CONTEXT
    msr cpsr_c, #(ABT_MODE | I_BIT | F_BIT)

    sub sp, #SIZEOF_ALT  
    str r1, [sp,#ALT_CPSR]
    str r2, [sp,#ALT_R0]
    str r3, [sp,#ALT_R1]
    str r4, [sp,#ALT_R2]
    str r5, [sp,#ALT_PC]

    mov r2, #SVC_MODE        
    and r1, #MODE_MASK    
    cmp r1, #USR_MODE
    moveq r2, #SYS_MODE
    orr r2, #(I_BIT | F_BIT)
    msr cpsr_c, r2
    mov sp, r6
    mov lr, r7

    msr cpsr_c, #(ABT_MODE | I_BIT | F_BIT)

  	ldr	r3, [r0,#CONTEXT_R3]
  	ldr	r4, [r0,#CONTEXT_R4]
	  ldr	r5, [r0,#CONTEXT_R5]
  	ldr	r6, [r0,#CONTEXT_R6]
  	ldr	r7, [r0,#CONTEXT_R7]
  	ldr	r8, [r0,#CONTEXT_R8]
  	ldr	r9, [r0,#CONTEXT_R9]
  	ldr	r10, [r0,#CONTEXT_R10]
  	ldr	r11, [r0,#CONTEXT_R11]
  	ldr	r12, [r0,#CONTEXT_R12]

    ldr r0, [sp, #ALT_CPSR]
    msr spsr, r0
    ldr r0, [sp, #ALT_R0]
    ldr r1, [sp, #ALT_R1]
    ldr r2, [sp, #ALT_R2]           
    ldr lr, [sp, #ALT_PC]

    add sp, #SIZEOF_ALT


.endm


#ifndef KERNEL_ARM_TASK_H
#define KERNEL_ARM_TASK_H

#include <kernel/types.h>
#include <kernel/error.h>
#include <kernel/arm/arm.h>



/*
 * x86-specific CPU state for each process
 *
 * Placed at beginning of Process structure so that it's easier to convert
 * between Process and TaskState in assembler routines.
 */

struct TaskState
{
    uint32 cpsr;                // Should be svc_mode spsr
    uint32 r0;
    uint32 pc;                  // Should be xyz_ svc_mode LR register. 
    uint32 r1;
    uint32 r2;
    uint32 r3;
    uint32 r4;
    uint32 r5;
    uint32 r6;
    uint32 r7;
    uint32 r8;
    uint32 r9;
    uint32 r10;
    uint32 r11;
    uint32 r12;
    uint32 sp;
    uint32 lr;
    
    bits32_t flags;
    struct CPU *cpu;
    int exception;
    vm_addr fault_addr;
    vm_addr fault_access;
    bits32_t dfsr;
} __attribute__ ((packed));



/*
 * Exception types
 */

#define EI_PAGEFAULT        0
#define EI_UNDEFSYSCALL     1
#define EI_UNDEFINSTR       2


#define MAX_CPU                     1
#define USER_STACK_SZ               65536
#define PROCESS_SZ                  1024






/* task_state.flags */

#define TSF_EXIT            (1<<0)
#define TSF_KILL            (1<<1)
#define TSF_PAGEFAULT       (1<<2)
#define TSF_EXCEPTION       (1<<3)





/*
 * struct CPU
 */

struct CPU
{
    struct Process *current_process;
    struct Process *idle_process;
    int reschedule_request;
    vm_addr svc_stack;
    vm_addr interrupt_stack;
    vm_addr exception_stack;
} __attribute__ ((packed));






/*
 * Globals
 */

extern struct CPU cpu_table[MAX_CPU];



struct CPU *GetCPU();
struct Process *GetCurrentProcess();


void __KernelExit (void);







#endif

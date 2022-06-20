#ifndef KERNEL_ARM_TASK_H
#define KERNEL_ARM_TASK_H

#include <kernel/arm/arm.h>
#include <kernel/error.h>
#include <kernel/types.h>
#include <stdint.h>
#include <sys/types.h>

/*
 * x86-specific CPU state for each process
 *
 * Placed at beginning of Process structure so that it's easier to convert
 * between Process and TaskState in assembler routines.
 */

struct UserContext {
  uint32_t sp;
  uint32_t lr;
  uint32_t cpsr; // Should be svc_mode spsr
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r4;
  uint32_t r5;
  uint32_t r6;
  uint32_t r7;
  uint32_t r8;
  uint32_t r9;
  uint32_t r10;
  uint32_t r11;
  uint32_t r12;
  uint32_t r0;
  uint32_t pc; // Should be xyz_ svc_mode LR register.
  uint32_t pad;
} __attribute__((packed));

struct TaskCatch {
  uint32_t pc;
} __attribute__((packed));

struct ExceptionState {
  bits32_t flags;
  //    struct CPU *cpu;
  int exception;
  vm_addr fault_addr;
  vm_addr fault_access;
  bits32_t dfsr;
};

/*
 * Exception types
 */

#define EI_PAGEFAULT 0
#define EI_UNDEFSYSCALL 1
#define EI_UNDEFINSTR 2

#define MAX_CPU 1
#define USER_STACK_SZ 65536
#define PROCESS_SZ 8192
#define NPROCESS 256
/* task_state.flags */

#define TSF_EXIT (1 << 0)
#define TSF_KILL (1 << 1)
#define TSF_PAGEFAULT (1 << 2)
#define TSF_EXCEPTION (1 << 3)

/*
 * struct CPU
 */

struct CPU {
  struct Process *current_process;
  struct Process *idle_process;
  int reschedule_request;
  vm_addr svc_stack;
  vm_addr interrupt_stack;
  vm_addr exception_stack;
} __attribute__((packed));

/*
 * Globals
 */

extern struct CPU cpu_table[MAX_CPU];

/*
 * Prototypes
 */
struct CPU *get_cpu();
struct Process *get_current_process();

void StartProcess(void);

#endif

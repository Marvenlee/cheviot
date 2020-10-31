
// Assembly equivalent of struct TaskState


.equ CONTEXT_SP,                   0        // USR_MODE SP
.equ CONTEXT_LR,                   4        // USR_MODE LR
.equ CONTEXT_CPSR,                 8        // USR_MODE CPSR 
.equ CONTEXT_R1,                   12
.equ CONTEXT_R2,                   16
.equ CONTEXT_R3,                   20
.equ CONTEXT_R4,                   24
.equ CONTEXT_R5,                   28
.equ CONTEXT_R6,                   32
.equ CONTEXT_R7,                   36
.equ CONTEXT_R8,                   40
.equ CONTEXT_R9,                   44
.equ CONTEXT_R10,                  48
.equ CONTEXT_R11,                  52
.equ CONTEXT_R12,                  56
.equ CONTEXT_R0,                   60      
.equ CONTEXT_PC,                   64       // PC return address  (current LR)
.equ CONTEXT_pad,                  68       // PAD
.equ SIZEOF_CONTEXT,               72       // Align up to 8 bytes


.equ ALT_PC,                        0
.equ ALT_R0,                        4
.equ ALT_R1,                        8
.equ ALT_R2,                        12
.equ ALT_CPSR,                      16
.equ SIZEOF_ALT,                    20


.equ TASK_CATCH_PC, 0
.equ SIZEOF_TASK_CATCH_CONTEXT,     4




.equ PROC_CPU,                  4



.equ EXCEPTION_FLAGS,                0
.equ EXCEPTION_EXCEPTION,            4
.equ EXCEPTION_PAGEFAULT_ADDR,       8
.equ EXCEPTION_PAGEFAULT_PROT,       12
.equ EXCEPTION_DFSR,                 16




.equ PROCESS_SZ,                8192



.equ SYSCALL_IDX_MASK,          63


// task_state.flags

.equ TSF_EXIT,                  (1<<0)
.equ TSF_KILL,                  (1<<1)
.equ TSF_PAGEFAULT,             (1<<2)
.equ TSF_UNDEFINSTR,            (1<<3)





// Assembly equivalent of struct CPU

.equ CPU_CURRENT_PROCESS,       0
.equ CPU_IDLE_PROCESS,          4
.equ CPU_RESCHEDULE_REQUEST,    8
.equ CPU_SVC_STACK,             12
.equ CPU_INTERRUPT_STACK,       16
.equ CPU_EXCEPTION_STACK,       20

.equ SIZEOF_CPU,                24



// cpu_table
 
.extern cpu_table




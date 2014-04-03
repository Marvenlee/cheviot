
// Assembly equivalent of struct TaskState


.equ TASK_CPSR,                 0
.equ TASK_R0,                   4       
.equ TASK_PC,                   8
.equ TASK_R1,                   12
.equ TASK_R2,                   16
.equ TASK_R3,                   20
.equ TASK_R4,                   24
.equ TASK_R5,                   28
.equ TASK_R6,                   32
.equ TASK_R7,                   36
.equ TASK_R8,                   40
.equ TASK_R9,                   44
.equ TASK_R10,                  48
.equ TASK_R11,                  52
.equ TASK_R12,                  56
.equ TASK_SP,                   60
.equ TASK_LR,                   64
.equ TASK_FLAGS,                68
.equ TASK_CPU,                  72
.equ TASK_EXCEPTION,            76
.equ TASK_PAGEFAULT_ADDR,       80
.equ TASK_PAGEFAULT_PROT,       84
.equ TASK_DFSR,                 88

.equ PROCESS_SZ,                1024



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




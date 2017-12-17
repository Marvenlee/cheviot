
.equ MODE_MASK, 0x1f

.equ USR_MODE,  0x10
.equ FIQ_MODE,  0x11
.equ IRQ_MODE,  0x12
.equ SVC_MODE,  0x13
.equ ABT_MODE,  0x17
.equ UND_MODE,  0x1b
.equ SYS_MODE,  0x1f

.equ I_BIT,     (1<<7)
.equ F_BIT,     (1<<6)








.equ PAGE_SIZE,                 4096




// Kernel - User VM Boundaries


.equ VM_KERNEL_BASE,            0xF0000000
.equ VM_KERNEL_CEILING,         0xFFFFF000
.equ VM_USER_BASE,              0x00001000
.equ VM_USER_CEILING,           0xEFC00000 
.equ VM_VSYS_BASE,              0xEFFF0000
.equ VM_VSYS_CEILING,           0xF0000000


.equ MAX_CPU,               8


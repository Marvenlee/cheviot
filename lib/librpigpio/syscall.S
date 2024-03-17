.extern __real_set_errno

.text

#define SYSCALL_SWI(name, idx)  \
.global name;                   \
name:;                          \
    swi idx;                    \
    bx lr;

#define SYSCALL_SWI5(name, idx)     \
.global name;                       \
name:;                              \
    sub sp, #8;                     \
  	str	r4, [sp, #0];               \
    ldr r4, [sp, #8];               \
    swi idx;                        \
    ldr	r4, [sp, #0];               \
    add sp, #8;                     \
    bx lr;

#define SYSCALL_SWI6(name, idx)     \
.global name;                       \
name:;                              \
    sub sp, #8;                     \
  	str	r4, [sp, #0];               \
  	str	r5, [sp, #4];               \
    ldr r4, [sp, #8];               \
    ldr r5, [sp, #12];              \
    swi idx;                        \
    ldr	r4, [sp, #0];               \
  	ldr	r5, [sp, #4];               \
    add sp, #8;                     \
    bx lr;


#define SYSCALL0 SYSCALL_SWI
#define SYSCALL1 SYSCALL_SWI
#define SYSCALL2 SYSCALL_SWI
#define SYSCALL3 SYSCALL_SWI
#define SYSCALL4 SYSCALL_SWI
#define SYSCALL5 SYSCALL_SWI5
#define SYSCALL6 SYSCALL_SWI6

SYSCALL2( _swi_rpi_configure_gpio, 96)
SYSCALL2( _swi_rpi_set_gpio, 97)



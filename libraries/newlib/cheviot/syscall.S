.extern __real_set_errno

.text

#define SYSCALL0(name)      \
.global name;               \
name:;                      \
    swi #SYS_ ## name;      \
    bx lr;

#define SYSCALL1 SYSCALL0
#define SYSCALL2 SYSCALL0
#define SYSCALL3 SYSCALL0
#define SYSCALL4 SYSCALL0




#define SYS_UnknownSyscall                  0

#define SYS_Debug                           1
SYSCALL1( Debug)

#define SYS_Spawn                           2
SYSCALL1( Spawn)

#define SYS_Exit                            3
SYSCALL1( Exit)

#define SYS_Join                            4
SYSCALL2( Join)

#define SYS_CloseHandle                     5
SYSCALL1( CloseHandle)

#define SYS_WaitFor                         6
SYSCALL1( WaitFor)

#define SYS_CheckFor                        7
SYSCALL1( CheckFor)

#define SYS_SetSchedParams                  8
SYSCALL3( SetSchedParams)

#define SYS_Yield                           9
SYSCALL0( Yield)

#define SYS_AddInterruptHandler             10
SYSCALL1( AddInterruptHandler)

#define SYS_MaskInterrupt                   11
SYSCALL1( MaskInterrupt)

#define SYS_UnmaskInterrupt                 12
SYSCALL1( UnmaskInterrupt)

#define SYS_CreateTimer                     13
SYSCALL0( CreateTimer)

#define SYS_SetTimer                        14
SYSCALL3( SetTimer)

#define SYS_GetSystemTime                   15
SYSCALL0( GetSystemTime)

#define SYS_SystemInfo                      16
SYSCALL1( SystemInfo)

#define SYS_VirtualAlloc                    17
SYSCALL3( VirtualAlloc)

#define SYS_VirtualAllocPhys                18
SYSCALL4( VirtualAllocPhys)

#define SYS_VirtualFree                     19
SYSCALL1( VirtualFree)

#define SYS_VirtualProtect                  20
SYSCALL2( VirtualProtect)

#define SYS_VirtualSizeOf                   21
SYSCALL1( VirtualSizeOf)

#define SYS_VirtualSwap                     22
SYSCALL2( VirtualSwap)

#define SYS_VirtualWire                     23
SYSCALL1( VirtualWire)

#define SYS_VirtualUnwire                   24
SYSCALL1( VirtualUnwire)

#define SYS_CreateChannel                   25
SYSCALL1( CreateChannel)

#define SYS_PutMsg                          26
SYSCALL3( PutMsg)

#define SYS_GetMsg                          27
SYSCALL3( GetMsg)

#define SYS_PutCondition                    28
SYSCALL2( PutCondition)

#define SYS_GetCondition                    29
SYSCALL1( GetCondition)

#define SYS_IsAChannel                      30
SYSCALL2( IsAChannel)

#define SYS_CoalesceVirtualMem              31
SYSCALL2( CoalesceVirtualMem)

#define SYS_CoalescePhysicalMem             32
SYSCALL2( CoalescePhysicalMem)

#define SYS_CompactPhysicalMem	            33
SYSCALL2( CompactPhysicalMem)


#define MAX_SYSCALL                         33







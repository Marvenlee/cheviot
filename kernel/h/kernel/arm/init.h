#ifndef KERNEL_ARM_INIT_H
#define KERNEL_ARM_INIT_H

#include <kernel/types.h>




void InitArm (void);
void InitProc (void);
void InitDebug (void);
void BootPrint(char *s, ...);
void BootPanic(char *s);



/*
 * init/init_vm.c
 */


void GetOptions (void);
void InitVM (void);
void InitMemSet (void *dst, char val, size_t size);
void *HeapAlloc (uint32 sz);
void MapKMem (vm_addr pbase, vm_addr pceiling, vm_addr vbase, uint32 pte_bits);


/*
 *
 */
 
void InitProcessTables (void); 
void InitRoot (void);
void InitIdleTasks (void);
void IdleTask (void);



void StartRootProcess (void);



#endif

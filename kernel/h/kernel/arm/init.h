#ifndef KERNEL_ARM_INIT_H
#define KERNEL_ARM_INIT_H

#include <kernel/types.h>

// Externs
extern uint8_t _stext;
extern uint8_t _ebss;
extern vm_addr _heap_base;
extern vm_addr _heap_current;

vm_addr core_pagetable_base;
vm_addr core_pagetable_ceiling;

// Prototypes
void InitMemoryMap(void);
void InitPageframeFlags(vm_addr base, vm_addr ceiling, bits32_t flags);
void CoalesceFreePageframes(void);
void InitPageCache(void);

void *IOMap(vm_addr pa, size_t sz, bool bufferable);

// We unmap the boot pagetables

// Move into bootinfo?
#define BOOT_BASE_ADDR 0x00001000
#define BOOT_CEILING_ADDR 0x00010000

void InitArm(void);
void InitProc(void);
void InitDebug(void);
void InitVM(void);
void KernelHeapInit(void);
void *KernelAlloc(vm_size size);
void InitProcesses(void);
void PopulateMemoryMap(void);
void InitIOPagetables(void);
void InitBufferCachePagetables(void);

void BootPrint(char *s, ...);
void BootPanic(char *s);

/*
 * init/init_vm.c
 */

void GetOptions(void);
void InitVM(void);
void *HeapAlloc(uint32_t sz);
void MapKMem(vm_addr pbase, vm_addr pceiling, vm_addr vbase, uint32_t pte_bits);

/*
 *
 */

void InitProcessTables(void);
void InitRoot(void);
void InitIdleTasks(void);
void IdleTask(void);

#endif

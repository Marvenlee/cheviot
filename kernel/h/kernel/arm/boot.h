#ifndef KERNEL_ARM_BOOT_H
#define KERNEL_ARM_BOOT_H

#include <kernel/types.h>


/*
 * Data structures passed to the kernel by bootloader, contains pointers
 * to a BSegment array of the memory map, and a list of loaded modules.
 */



struct BSegment
{
    uint32 base;
    uint32 ceiling;
    int8 type;          // free, resv, anon
    uint32 flags;
};


struct BModule
{
    char name[64];
    void *addr;
    size_t size;
};


struct BootInfo
{
    void (*procman_entry_point)(void);
    vm_addr user_stack_base;
    vm_addr user_stack_ceiling;
    vm_addr user_base;
    vm_addr heap_base;
    vm_addr heap_ceiling;
    uint32 screen_width;
    uint32 screen_height;
    uint32 screen_buf;
    uint32 screen_pitch;
 
    struct BSegment *segment_table;
    int segment_cnt;

    struct BModule *module_table;
    int module_cnt;
};







#endif

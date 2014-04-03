#ifndef BOOTINFO_H
#define BOOTINFO_H

#include "types.h"



#define NMODULE		64

struct Module
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
 
	struct Segment *segment_table;
	int segment_cnt;

	struct Module *module_table;
	int module_cnt;
};






#endif

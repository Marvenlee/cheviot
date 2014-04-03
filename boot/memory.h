#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"





#define NSEGMENT 			256


#define PROT_NONE				0
#define PROT_READ				(1<<0)
#define PROT_WRITE				(1<<1)
#define PROT_EXEC				(1<<2)
#define PROT_ALL				(PROT_READ | PROT_WRITE | PROT_EXEC)
#define PROT_READWRITE 			(PROT_READ | PROT_WRITE)
#define PROT_READEXEC			(PROT_READ | PROT_EXEC)
#define MAP_FIXED				(1<<3)


/*
 * Types of physical memory segments that are part of memory map.
 */

#define SEG_TYPE_RESERVED			0
#define SEG_TYPE_FREE				1
#define SEG_TYPE_ALLOC				2







/*
 * struct Segment
 */
 
struct Segment
{
	vm_addr base;
	vm_addr ceiling;
	int8 type;			// free, resv, anon
	uint32 flags;
};












#define ALIGN_UP(val, alignment)								\
	((((val) + (alignment) - 1)/(alignment))*(alignment))
	

#define ALIGN_DOWN(val, alignment)								\
			((val) - ((val) % (alignment)))
			
			

void *bmalloc (size_t size);
void bfree (void *mem);
void bmemcpy (void *dst, const void *src, size_t nbytes);
void bmemset (void *dst, char v, size_t nbytes);

void init_mem(void);
void *SegmentCreate (vm_offset addr, vm_size size, int type, bits32_t flags);
void SegmentInsert (int index, int cnt);
struct Segment *SegmentFind (vm_addr addr);
void SegmentCoalesce (void);
struct Segment *SegmentAlloc (vm_size size, uint32 flags, vm_addr *ret_addr);




#endif

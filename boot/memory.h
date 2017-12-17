#ifndef MEMORY_H
#define MEMORY_H

#include "types.h"


#define ALIGN_UP(val, alignment)								\
	((((val) + (alignment) - 1)/(alignment))*(alignment))
	

#define ALIGN_DOWN(val, alignment)								\
			((val) - ((val) % (alignment)))
			
			

void *bmalloc (size_t size);
void bfree (void *mem);
void bmemcpy (void *dst, const void *src, size_t nbytes);
void bmemset (void *dst, char v, size_t nbytes);
vm_size init_mem(void);

#endif

#include "types.h"
#include "memory.h"
#include "globals.h"
#include "arm.h"
#include "dbg.h"


// Variables
extern uint8 _bss_end;
static vm_addr heap_top_addr = 0;


/*
    Simple incrementing memory allocator
*/
void *bmalloc (size_t size)
{
  	void *mem;
	
	heap_top_addr = ALIGN_UP (heap_top_addr, 16);	
	mem = (void *)heap_top_addr;	
	heap_top_addr += size;

    KLog ("bmalloc addr = %08x, size = %d", (vm_addr)mem, size);	
	return mem;
}


/*!
    Some places in emmc.c driver call bfree().  We do nothing.    
*/
void bfree (void *mem)
{
}


/*!
    Simple memcpy (could use built-in)
 */
void bmemcpy (void *dst, const void *src, size_t nbytes)
{
	char *dst1 = dst;
	const char *src1 = src;
    
	while (nbytes > 0)
	{
		*dst1++ = *src1++;
		nbytes --;
	}
}


/*!
    Simple memset (could use built-in)
 */
void bmemset (void *dst, char val, size_t nbytes)
{
	char *dst1 = dst;
    		
	while (nbytes > 0)
	{
		*dst1++ = val;
		nbytes--;
	}
}


/*
    Initialise bmalloc() heap,  determine amount of physical memory.
 */
vm_size init_mem(void)
{
	uint32 result;
    vm_size size;
    
    heap_top_addr = ALIGN_UP ((vm_addr)&_bss_end, 32);

	mailbuffer[0] = 8 * 4;
	mailbuffer[1] = 0;
	mailbuffer[2] = 0x00010005;
	mailbuffer[3] = 8;
	mailbuffer[4] = 0;
	mailbuffer[5] = 0;
	mailbuffer[6] = 0;
	mailbuffer[7] = 0;

	do
	{
		MailBoxWrite ((uint32)mailbuffer, 8);
		result = MailBoxRead (8);
	} while (result == 0);	

    // TODO: Assuming memory starts at 0x00000000
    //	mem_base = mailbuffer[5];
	size = mailbuffer[6];
    return size;
}



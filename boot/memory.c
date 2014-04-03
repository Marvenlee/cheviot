#include "types.h"
#include "memory.h"
#include "globals.h"
#include "arm.h"

char heap[32768];
size_t sbrk_offset = 0;

	
void *bmalloc (size_t size)
{
	void *mem;
	
	sbrk_offset = ALIGN_UP (sbrk_offset, 16);
	
	mem = (void *)&heap[sbrk_offset];
	
	sbrk_offset += size;
	
	return mem;
}


void bfree (void *mem)
{
}



/*
 *
 */

void bmemcpy (void *dst, const void *src, size_t nbytes)
{
	char *dst1;
	const char *src1;
	src1 = src;
	dst1 = dst;


	while (nbytes > 0)
	{
		*dst1++ = *src1++;
		nbytes --;
	}
}




/*
 *
 */

void bmemset (void *dst, char val, size_t nbytes)
{
	char *dst1;
		
	while (nbytes > 0)
	{
		*dst1++ = val;
		nbytes--;
	}
}




/*
 *
 */

void init_mem(void)
{
	uint32 result;

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
	
	segment_cnt = 1;
	segment_table[0].type = SEG_TYPE_FREE;
	segment_table[0].flags = 0;
	segment_table[0].base = mailbuffer[5];
	segment_table[0].ceiling = mailbuffer[6];
}









void *SegmentCreate (vm_offset addr, vm_size size, int type, bits32_t flags)
{
	struct Segment *seg;
	vm_addr base, ceiling;
	int t;

	addr = ALIGN_DOWN (addr, 4096);
	size = ALIGN_UP (size, 4096);

	if (flags & MAP_FIXED)
	{	
		if ((seg = SegmentFind(addr)) == NULL)
			return NULL;
			
		if ((addr + size > seg->ceiling) || seg->type != SEG_TYPE_FREE)
			return NULL;
	}
	else if ((seg = SegmentAlloc (size, flags, &addr)) == NULL)
	{
		return NULL;
	}



	
	t = seg - segment_table;

	if (seg->base == addr && seg->ceiling == addr + size)
	{
		/* Perfect fit, initialize region */
		
		seg->base = addr;
		seg->ceiling = addr + size;
		seg->type = type;
		seg->flags = flags;
	}
	else if (seg->base < addr && seg->ceiling > addr + size)
	{
		/* In the middle, between two parts */
	
		base = seg->base;
		ceiling = seg->ceiling;
	
		SegmentInsert (t, 2);
				
		seg->base = base;
		seg->ceiling = addr;
		seg->type = SEG_TYPE_FREE;
		
		seg++;
		
		seg->base = addr;
		seg->ceiling = addr + size;
		seg->type = type;
		seg->flags = flags;
		
		seg++;
		
		seg->base = addr + size;
		seg->ceiling = ceiling;
		seg->type = SEG_TYPE_FREE;
	}
	else if (seg->base == addr && seg->ceiling > addr + size)
	{
		/* Starts at bottom of area */
	
		base = seg->base;
		ceiling = seg->ceiling;
	
		SegmentInsert (t, 1);
		
		seg->base = addr;
		seg->ceiling = addr + size;
		seg->type = type;
		seg->flags = flags;
		
		seg++;
		
		seg->base = addr + size;
		seg->ceiling = ceiling;
		seg->type = SEG_TYPE_FREE;
	}
	else
	{
		/* Starts at top of area */
		
		base = seg->base;
		ceiling = seg->ceiling;
	
		SegmentInsert (t, 1);
		
		seg->base = base;
		seg->ceiling = addr;
		seg->type = SEG_TYPE_FREE;
		
		seg++;
		
		seg->base = addr;
		seg->ceiling = addr + size;
		seg->type = type;
		seg->flags = flags;
	}
	
	
	return (void *)addr;
}




/*
 * MemAreaInsert();
 *
 * Replace code with a memmove/memcpy() ?
 *
 * May want to optimize to avoid moving entries if 'cnt' contiguous areas are free
 * starting at idx. Coalesce them if possible?
 * 
 */
 
void SegmentInsert (int index, int cnt)
{
	int t;
	
	for (t = segment_cnt - 1 + cnt; t >= (index + cnt); t--)
	{
		segment_table[t].base = segment_table[t-cnt].base;
		segment_table[t].ceiling = segment_table[t-cnt].ceiling;
		segment_table[t].type = segment_table[t-cnt].type;
		segment_table[t].flags = segment_table[t-cnt].flags;
	}
	
	segment_cnt += cnt;
}




/*
 *
 */

struct Segment *SegmentFind (vm_addr addr)
{
	int low = 0;
	int high = segment_cnt - 1;
	int mid;	
	
	
	while (low <= high)
	{
		mid = low + ((high - low) / 2);
		
		if (addr >= segment_table[mid].ceiling)
			low = mid + 1;
		else if (addr < segment_table[mid].base)
			high = mid - 1;
		else
			return &segment_table[mid];
	}

	return NULL;
}




/*
 * MemAreaCoalesce();
 */
 
void SegmentCoalesce (void)
{
	int t, s;

	for (t = 0, s = 0; t < segment_cnt; t++)
	{
		if (s > 0 && (segment_table[s-1].type == SEG_TYPE_FREE && segment_table[t].type == SEG_TYPE_FREE))
		{
			segment_table[s-1].ceiling = segment_table[t].ceiling;
		}
		else
		{
			segment_table[s].base 	     = segment_table[t].base;
			segment_table[s].ceiling     = segment_table[t].ceiling;
			segment_table[s].type 	     = segment_table[t].type;
			segment_table[s].flags 	     = segment_table[t].flags;
			s++;
		}
	}

	segment_cnt = s;
}






/*
 * FIXME:  Allocate above *ret_addr
 */

struct Segment *SegmentAlloc (vm_size size, uint32 flags, vm_addr *ret_addr)
{
	int i;
	vm_addr addr;
	
	
	addr = *ret_addr;
		
	for (i = 0; i < segment_cnt; i++)
	{
		if (segment_table[i].type != SEG_TYPE_FREE)
			continue;
			
		if (segment_table[i].base <= addr &&
			addr + size < segment_table[i].ceiling)
		{
			*ret_addr = addr;
			return &segment_table[i];
		}
		
		if (segment_table[i].base >= addr &&
			size <= segment_table[i].ceiling - segment_table[i].base)
		{
			*ret_addr = segment_table[i].base;
			return &segment_table[i];
		}
	}
	
	return NULL;
}











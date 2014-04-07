#include <sys/syscalls.h>
#include <sys/marshall.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/debug.h>


#define ALIGN_UP(val, alignment)								\
	((((val) + (alignment) - 1)/(alignment))*(alignment))
#define ALIGN_DOWN(val, alignment)								\
			((val) - ((val) % (alignment)))
			
			

#define MEM_ALIGNMENT		16



/*
 * struct MarshallHeader
 */

struct MarshallHeader
{
	size_t size;
	int type;
};




/*
 * MarshallBegin();
 */
 
void MarshallBegin (marshall_t *mm)
{
	mm->buf = NULL;
	mm->size = 0;
	mm->current = NULL;
	mm->error = 0;
	
	LIST_INIT (&mm->marshallent_list);
}




/*
 * MarshallEnd();
 */
 
void *MarshallEnd (marshall_t *mm)
{
	struct MarshallHeader *hdr;
	struct MarshallEnt *me;
	uint8 *current;
	size_t total_size;
	
	total_size = 0;
	me = LIST_HEAD(&mm->marshallent_list);
	
	while (me != NULL)
	{
		total_size += ALIGN_UP(sizeof (struct MarshallHeader), MEM_ALIGNMENT);
		total_size += ALIGN_UP(me->size, MEM_ALIGNMENT);
		me = LIST_NEXT (me, link);
	}
	
	total_size += ALIGN_UP(sizeof (struct MarshallHeader), MEM_ALIGNMENT);
	mm->size = total_size;
	
	if (mm->error == 0)
	{
		if ((mm->buf = VirtualAlloc (NULL, mm->size, PROT_READWRITE)) == NULL)
		{
			mm->error = 1;
		}
	}
	
	if (mm->error == 1)
	{
		while ((me = LIST_HEAD(&mm->marshallent_list)) != NULL)
		{
			LIST_REM_HEAD (&mm->marshallent_list, link);
			free (me);
		}
		
		return NULL;
	}
	
	current = mm->buf;
	me = LIST_HEAD(&mm->marshallent_list);
	
	while (me != NULL)
	{
		hdr = (struct MarshallHeader *)current;
		hdr->size = me->size;
		
		current += ALIGN_UP (sizeof (*hdr), MEM_ALIGNMENT);
		memcpy (current, me->addr, me->size);
		
		current += ALIGN_UP (me->size, MEM_ALIGNMENT);
		me = LIST_NEXT(me, link);
	}
		
	while ((me = LIST_HEAD(&mm->marshallent_list)) != NULL)
	{
		LIST_REM_HEAD (&mm->marshallent_list, link);
		free (me);
	}
	
	return mm->buf;
}



/*
 *
 */
 
void MarshallFreeManifest (marshall_t *mm)
{
	struct MarshallEnt *me;

	while ((me = LIST_HEAD(&mm->marshallent_list)) != NULL)
	{
		LIST_REM_HEAD (&mm->marshallent_list, link);
		free (me);
	}
	
	mm->buf = NULL;
	mm->size = 0;
}






/*
 * MarshallString();
 */

void MarshallString (marshall_t *mm, const char *str)
{
	size_t size;
	struct MarshallEnt *me;
	
	size = strlen(str) + 1;
	
	if (mm->error == 1 || (me = malloc(sizeof *me)) == NULL)
	{
		mm->error = 1;
		return;
	}
	
	me->addr = (char *)str;
	me->size = size;
	
	LIST_ADD_TAIL (&mm->marshallent_list, me, link);
}




/*
 * MarshallItem();
 */

void MarshallItem (marshall_t *mm, void *item, size_t size)
{
	struct MarshallEnt *me;
	
	if (mm->error == 1 || (me = malloc(sizeof *me)) == NULL)
	{
		mm->error = 1;
		return;
	}
	
	me->addr = item;
	me->size = size;
	
	LIST_ADD_TAIL (&mm->marshallent_list, me, link);
}




/*
 * UnmarshallBegin();
 */

void UnmarshallBegin (marshall_t *mm, void *buf, size_t size)
{
	mm->buf = buf;
	mm->size = size;
	mm->current = buf;
}




/*
 * UnmarshallString();
 */

char *UnmarshallString (marshall_t *mm)
{
	size_t size;
	struct MarshallHeader *hdr;
	char *str;
	size_t remaining;

	if (mm->error == 1)
		return NULL;

	remaining = mm->size - (mm->current - mm->buf);

	if (remaining <= sizeof *hdr)
	{
		mm->error = 1;
		return NULL;
	}
	
	hdr = mm->current;

	if (hdr->size <= 0)
		return NULL;
		
	mm->current += ALIGN_UP (sizeof *hdr, MEM_ALIGNMENT);

	remaining = mm->size - (mm->current - mm->buf);

	if (remaining < hdr->size)
	{
		mm->error = 1;
		return NULL;
	}

	str = mm->current;
	str[hdr->size] = '\0';
	
	mm->current += ALIGN_UP (hdr->size, MEM_ALIGNMENT);
	
	return str;
}




/*
 * UnmarshallItem();
 */

void *UnmarshallItem (marshall_t *mm, size_t *rsize)
{
	size_t size;
	struct MarshallHeader *hdr;
	void *item;	
	size_t remaining;
	
	if (mm->error == 1 || rsize == NULL)
		return NULL;
	
	remaining = mm->size - (mm->current - mm->buf);

	if (remaining <= sizeof *hdr)
	{
		mm->error = 1;
		return NULL;
	}
	
	hdr = mm->current;

	if (hdr->size <= 0)
	{
		mm->error = 1;
		return NULL;
	}
	
	mm->current += ALIGN_UP (sizeof *hdr, MEM_ALIGNMENT);

	remaining = mm->size - (mm->current - mm->buf);

	if (remaining < hdr->size)
	{
		mm->error = 1;
		return NULL;
	}
	
	item = mm->current;
	mm->current += ALIGN_UP (hdr->size, MEM_ALIGNMENT);
	
	*rsize = hdr->size;
	return item;
}






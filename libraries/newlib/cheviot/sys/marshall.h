#ifndef	_SYS_MARSHALL_H
#define	_SYS_MARSHALL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <sys/lists.h>



struct MarshallEnt
{
	LIST_ENTRY (MarshallEnt) link;
	void *addr;
	size_t size;
	
};
 

typedef struct
{
	void *buf;
	size_t size;
	void *current;
	int error;
	LIST (MarshallEnt) marshallent_list;
} marshall_t;




void MarshallBegin (marshall_t *mm);
void *MarshallEnd (marshall_t *mm);
void MarshallFreeManifest (marshall_t *mm);
void MarshallString (marshall_t *mm, const char *str);
void MarshallItem (marshall_t *mm, void *item, size_t size);
void UnmarshallBegin (marshall_t *mm, void *buf, size_t size);
char *UnmarshallString (marshall_t *mm);
void *UnmarshallItem (marshall_t *mm, size_t *rsize);














#ifdef __cplusplus
}
#endif
#endif /* _SYS_MARSHALL_H */

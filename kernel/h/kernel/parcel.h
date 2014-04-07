#ifndef KERNEL_PARCEL_H
#define KERNEL_PARCEL_H

#include <kernel/types.h>
#include <kernel/lists.h>



/*
 * Forward declarations
 */
struct MemArea;



/*
 * Parcel types
 */
 
#define PARCEL_NONE         0
#define PARCEL_MSG          1
#define PARCEL_HANDLE       2




struct Parcel
{
    int type;
    LIST_ENTRY (Parcel) link;
    
    union
    {
        int handle;
        struct VirtualSegment *virtualsegment;
    } content;
};



LIST_TYPE (Parcel) parcel_list_t;





#endif

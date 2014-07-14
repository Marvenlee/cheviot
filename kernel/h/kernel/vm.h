#ifndef KERNEL_VM_H
#define KERNEL_VM_H

#include <kernel/types.h>
#include <kernel/lists.h>
#include <kernel/arch.h>
#include <kernel/parcel.h>




/*
 * Free segment list and cache hash table size
 */
 
#define NSEGBUCKET        10
#define CACHE_HASH_SZ   1024



/*
 * VirtualAlloc flags
 */

#define PROT_NONE               0
#define PROT_READ               (1<<0)
#define PROT_WRITE              (1<<1)
#define PROT_EXEC               (1<<2)
#define PROT_ALL                (PROT_READ | PROT_WRITE | PROT_EXEC)
#define PROT_READWRITE          (PROT_READ | PROT_WRITE)
#define PROT_READEXEC           (PROT_READ | PROT_EXEC)

#define MAP_FIXED               (1<<3)      // No longer needed?
#define MAP_VERSION             (1<<4)
// #define MAP_NONBLOCK            (1<<5)

#define CACHE_DEFAULT           (0<<8)
#define CACHE_WRITEBACK         (1<<8)
#define CACHE_WRITETHRU         (2<<8)
#define CACHE_WRITECOMBINE      (3<<8)
#define CACHE_UNCACHEABLE       (4<<8)
#define CACHE_WEAKUNCACHEABLE   (5<<8)
#define CACHE_WRITEPROTECT      (6<<8)

#define NCACHE_POLICIES         7

#define SEG_KERNEL             (1 << 18)
#define SEG_WIRED              (1 << 19)    
#define SEG_COMPACT            (1 << 20)

#define MEM_RESERVED            (0<<29)
#define MEM_GARBAGE             (1<<29)
#define MEM_ALLOC               (2<<29)
#define MEM_PHYS                (3<<29)
#define MEM_FREE                (4<<29)

#define MEM_MASK                0xE0000000
#define MAP_MASK                (MAP_FIXED | MAP_VERSION | MAP_NONBLOCK)
#define PROT_MASK               0x00000007
#define CACHE_MASK              0x00000f00

#define VM_SYSTEM_MASK          (MEM_MASK | SEG_KERNEL | SEG_WIRED | SEG_COMPACT)

#define MEM_TYPE(flags)         (flags & MEM_MASK)




/*
 * PutCachedSegment() flags
 */

#define CACHE_AGE               (1<<0)


/*
 *
 */
 
#define VM_WATCHDOG_SECONDS         20
#define MAX_VM_RETRIES              3






/*
 *
 */

struct Segment
{
    vm_addr base;
    vm_size size;
    vm_addr physical_addr;
    struct Process *owner;
    bits32_t flags;
    
    // With 64k entries, these could be converted to 16-bit indices instead.

    int bucket_q;     
        
    LIST_ENTRY (Segment) link;         // Free and heap list
    LIST_ENTRY (Segment) lru_link;          // Cache LRU list entry  (double link);
    LIST_ENTRY (Segment) hash_link;         // Cache lookup hash table (via segment version)
    uint64 segment_id;
};


;


HEAP_TYPE (Segment) seg_heap_t;

LIST_TYPE (Segment) seg_list_t;





// vm/addressspace.c

void InitAddressSpace (struct Process *proc);
void FreeAddressSpace (void);


// vm/segment.c

struct Segment *SegmentFind (vm_addr addr);
int SegmentFindMultiple (struct Segment **seg_table, void **mem_ptrs, int nseg);
struct Segment *SegmentAlloc (ssize_t size, bits32_t flags);
struct Segment *SegmentAllocPhys (ssize_t size, bits32_t flags);
void SegmentFree (struct Segment *seg);


SYSCALL vm_addr GetCachedSegment (uint64 *in_version);
SYSCALL int PutCachedSegment (vm_addr addr, bits32_t flags, uint64 *out_version);
SYSCALL int ExpungeCachedSegment (uint64 *in_segment_id);

void VMTaskBegin(void);
void VMTaskResizeCache (void);
void VMTaskCompactHeap(void);
void VMTaskAllocateSegments (void);
struct Segment *AllocateSegment (struct Process *proc, vm_size size, bits32_t flags);




// arch/pmap.c

int PmapSupportsCachePolicy (bits32_t flags);
int PmapInit (struct Process *proc);
void PmapDestroy (struct Process *proc);
void PmapEnterRegion (struct Pmap *pmap, struct Segment *seg, vm_addr va);
void PmapEnterPhysicalSpace (struct Pmap *pmap, vm_addr addr);
void PmapRemoveRegion (struct Pmap *pmap, struct Segment *seg);
void PmapFlushTLBs (void);
void PmapInvalidateAll (void);
void PmapSwitch (struct Process *next, struct Process *current);

vm_size PmapAllocAlignment (vm_size size);


// vm/vm.c

SYSCALL vm_offset VirtualAlloc (vm_addr addr, ssize_t len, bits32_t flags);
void VirtualAllocContinuation (void);
SYSCALL vm_offset VirtualAllocPhys (vm_addr addr, ssize_t len, bits32_t flags, vm_addr paddr);
SYSCALL int VirtualFree (vm_offset addr);
SYSCALL int VirtualProtect (vm_addr addr, uint32 prot);
SYSCALL ssize_t VirtualSizeOf (vm_addr addr);
SYSCALL int VirtualVersion (vm_addr addr, uint64 *version);
SYSCALL int WireMem (vm_addr addr, vm_addr *phys);
SYSCALL int UnwireMem (vm_addr addr);


// arch/memcpy.s


void CopyIn  (void *dst, const void *src, size_t sz);
void CopyOut (void *dst, const void *src, size_t sz);
void MemSet (void *mem, int c, size_t sz);
void MemCpy (void *dst, const void *src, size_t sz);





/*
 * VM Macros
 */

#define ALIGN_UP(val, alignment)                                \
    ((((val) + (alignment) - 1)/(alignment))*(alignment))
    
    
    
#define ALIGN_DOWN(val, alignment)                              \
            ((val) - ((val) % (alignment)))



#endif
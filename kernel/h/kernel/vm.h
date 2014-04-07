#ifndef KERNEL_VM_H
#define KERNEL_VM_H

#include <kernel/types.h>
#include <kernel/lists.h>
#include <kernel/arch.h>
#include <kernel/parcel.h>






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

#define MAP_FIXED               (1<<3)

#define MAP_VERSION             (1<<5)

#define CACHE_DEFAULT           (0<<8)
#define CACHE_WRITEBACK         (1<<8)
#define CACHE_WRITETHRU         (2<<8)
#define CACHE_WRITECOMBINE      (3<<8)
#define CACHE_UNCACHEABLE       (4<<8)
#define CACHE_WEAKUNCACHEABLE   (5<<8)
#define CACHE_WRITEPROTECT      (6<<8)

#define NCACHE_POLICIES         7


#define MEM_RESERVED            (0<<30)
#define MEM_FREE                (1<<30)
#define MEM_ALLOC               (2<<30)
#define MEM_PHYS                (3<<30)

#define MEM_MASK                0xc0000000
#define MAP_MASK                (MAP_FIXED | MAP_WIRED | MAP_NOX64 | MAP_BELOW16M | MAP_BELOW4G)
#define PROT_MASK               0x00000007
#define CACHE_MASK              0x00000f00

#define VM_SYSTEM_MASK          (MEM_MASK)

#define MEM_TYPE(flags)         (flags & MEM_MASK)



/*
 * Types of physical memory segments that are part of memory map.
 */

#define SEG_TYPE_RESERVED           0
#define SEG_TYPE_FREE               1
#define SEG_TYPE_ALLOC              2







/*
 *
 */

struct VirtualSegment
{
    vm_addr base;
    vm_addr ceiling;
    vm_addr physical_addr;
    bits32_t flags;
    uint64 version;  
    struct Parcel *parcel;
    struct Process *owner;
    bool busy;
    bool wired;
};




/*
 * struct Segment
 */
 
struct PhysicalSegment
{
    vm_addr base;
    vm_addr ceiling;
    int8 type;          // free, resv, anon
    vm_addr virtual_addr;
};











// vm/addressspace.c

void InitAddressSpace (struct Process *proc);
void FreeAddressSpace (void);

// vm/segment.c

struct PhysicalSegment *PhysicalSegmentCreate (vm_offset addr, vm_size size, int type);
void PhysicalSegmentDelete (struct PhysicalSegment *seg);
void PhysicalSegmentInsert (int index, int cnt);
struct PhysicalSegment *PhysicalSegmentFind (vm_addr addr);
struct PhysicalSegment *PhysicalSegmentBestFit (vm_size size, uint32 flags, vm_addr *ret_addr);
SYSCALL int CoalescePhysicalMem (void);
SYSCALL vm_addr CompactPhysicalMem (vm_addr addr);

// vm/area.c

SYSCALL int CoalesceVirtualMem (void);
struct VirtualSegment *VirtualSegmentCreate (vm_offset addr, vm_size size, bits32_t flags);
void VirtualSegmentDelete (struct VirtualSegment *vs);
struct VirtualSegment *VirtualSegmentFind (vm_addr addr);
void VirtualSegmentInsert (int index, int cnt);
struct VirtualSegment *VirtualSegmentBestFit (vm_size size, uint32 flags, vm_addr *addr);
int VirtualSegmentFindMultiple (struct VirtualSegment **vs_table, void **mem_ptrs, int nseg);
SYSCALL int CoalesceVirtualMem (void);

// arch/pmap.c

int PmapSupportsCachePolicy (bits32_t flags);
int PmapInit (struct Pmap *pmap);
void PmapDestroy (struct Pmap *pmap);
void PmapEnterRegion (struct Pmap *pmap, struct VirtualSegment *vs, vm_addr va);
void PmapRemoveRegion (struct Pmap *pmap, struct VirtualSegment *vs);
void PmapFlushTLBs (void);
void PmapInvalidateAll (void);
void PmapSwitch (struct Pmap *next, struct Pmap *pmap);


// vm/virtualalloc.c

SYSCALL vm_offset VirtualAlloc (vm_addr addr, vm_size len, bits32_t flags);
SYSCALL vm_offset VirtualAllocPhys (vm_addr addr, vm_size len, bits32_t flags, vm_addr paddr);
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
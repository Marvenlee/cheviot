#ifndef KERNEL_VM_H
#define KERNEL_VM_H

#include <kernel/types.h>
#include <kernel/lists.h>
#include <kernel/arch.h>




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

#define MAP_LAZY                (1<<4)
#define MAP_PHYS                (1<<5)
#define MAP_CONTIGUOUS          (1<<6)
#define MAP_WIRED               (1<<7)

#define CACHE_DEFAULT           (0<<8)
#define CACHE_WRITEBACK         (1<<8)
#define CACHE_WRITETHRU         (2<<8)
#define CACHE_WRITECOMBINE      (3<<8)
#define CACHE_UNCACHEABLE       (4<<8)
#define CACHE_WEAKUNCACHEABLE   (5<<8)
#define CACHE_WRITEPROTECT      (6<<8)

#define NCACHE_POLICIES         7


// Flags used for kernel administration of pages
#define MEM_RESERVED            (0<<28)
#define MEM_GARBAGE             (1<<28)
#define MEM_ALLOC               (2<<28)
#define MEM_PHYS                (3<<28)
#define MEM_FREE                (4<<28)

#define MAP_COW                 (1<<26)
#define MAP_USER                (1<<27)

#define MEM_MASK                0xF0000000
#define MAP_MASK                (MAP_PHYS | MAP_CONTIGUOUS)
#define PROT_MASK               0x0000000F
#define CACHE_MASK              0x00000F00

#define VM_SYSTEM_MASK          (MEM_MASK | MAP_COW | MAP_USER)

#define MEM_TYPE(flags)         (flags & MEM_MASK)




#define CACHE_AGE               (1<<0)
#define CACHE_BUSY				(1<<0)


/*
 *
 */
 
#define PGF_INUSE					(1<<0)
#define PGF_RESERVED				(1<<1)
#define PGF_CLEAR					(1<<2)
#define PGF_KERNEL					(1<<3)
#define PGF_USER					(1<<4)
#define PGF_PAGETABLE				(1<<5)



struct VPage
{
    LIST_ENTRY (VPage) link;
    struct Pageframe *pf;
};


struct Pageframe
{
    vm_size size;                       // Pageframe is either 64k, 16k, or 4k    
    vm_addr physical_addr;
    int reference_cnt;                  // Count of vpage references.
	bits32_t flags;
    LIST_ENTRY  (Pageframe) link;		// cache lru, busy link.  (busy and LRU on separate lists?)
    
    struct PmapPageframe pmap_pageframe;
};




struct AddressSpace
{
	struct Pmap *pmap;
};


struct Pagecache
{
    int type;
    int reference_cnt;
    
    LIST_ENTRY (Pagecache) link;
    
    union 
    {
        struct {
            void *vnode;
            size_t offset;
            // bits32_t present;   ???????  or use kernel pagetables to detect presence
        } file;
        
        struct {
            struct Pmap *pmap;
            vm_addr va;
            // Number of pagetables???????????????
        } pagetable;
        
        struct {
            struct Process *process;
        } pagedir;
    } u;
};



LIST_TYPE (Pageframe) pageframe_list_t;
LIST_TYPE (Pagecache) pagecache_list_t;

#define PC_TYPE_FREE 0
#define PC_TYPE_PAGETABLE 1
#define PC_TYPE_PAGEDIR 2
#define PC_TYPE_FILE 3


#define NPAGECACHE_HASH 128

//#define NSLAB_SZ 3

#define SLAB_4K_SZ  0
#define SLAB_16K_SZ 1
#define SLAB_64K_SZ 2



// vm/addressspace.c

int InitAddressSpace (struct AddressSpace *as);
int ForkAddressSpace (struct AddressSpace *new_as, struct AddressSpace *old_as);
void FreeAddressSpace (struct AddressSpace *as);



// arch/pmap.c

int PmapSupportsCachePolicy (bits32_t flags);
int PmapInit (struct AddressSpace *as);
void PmapDestroy (struct AddressSpace *as);

int PmapFork (struct AddressSpace *new_as, struct AddressSpace *old_as);

int PmapKEnter (vm_addr va, vm_addr pa, bits32_t flags);

int PmapEnter (struct AddressSpace *as, vm_addr addr, vm_addr paddr, bits32_t flags);
int PmapRemove (struct AddressSpace *as, vm_addr addr);
int PmapRemoveAll (struct AddressSpace *as);

int PmapProtect (struct AddressSpace *as, vm_addr addr, bits32_t flags);
void PmapFlushTLBs (void);
void PmapInvalidateAll (void);
void PmapSwitch (struct Process *next, struct Process *current);

vm_size PmapAllocAlignment (vm_size size);

void PmapPageframeInit (struct PmapPageframe *ppf);

int PmapGetPhysicalAddr (struct AddressSpace *as, vm_addr va, vm_addr *pa);
vm_addr PmapPfToPa (struct Pageframe *pf);
struct Pageframe *PmapPaToPf (vm_addr pa);
vm_addr PmapVaToPa (vm_addr vaddr);
vm_addr PmapPaToVa (vm_addr paddr);
vm_addr PmapPfToVa (struct Pageframe *pf);
struct Pageframe *PmapVaToPf (vm_addr va);

int PmapExtract (struct AddressSpace *as, vm_addr va, vm_addr *pa, uint32 *flags);

uint32 *PmapAllocPageDirectory (void);



// vm/pagefault.c

int PageFault (vm_addr addr, bits32_t access);

// vm/vm.c

SYSCALL vm_size VirtualAlloc (vm_addr addr, vm_size len, bits32_t flags);
SYSCALL vm_size VirtualAllocPhys (vm_addr addr, vm_size len, bits32_t flags, vm_addr paddr);
SYSCALL vm_size VirtualFree (vm_addr addr, vm_size len);
SYSCALL vm_size VirtualProtect (vm_addr addr, vm_size len, bits32_t flags);



// arch/memcpy.s

void CopyIn  (void *dst, const void *src, size_t sz);
void CopyOut (void *dst, const void *src, size_t sz);
void MemSet (void *mem, int c, size_t sz);
void MemCpy (void *dst, const void *src, size_t sz);


// pageframe.c

struct Pageframe *AllocPageframe (vm_size);
void FreePageframe (struct Pageframe *pf);
struct Pageframe *PhysicalAddrToPageframe (vm_addr pa);

void *KMalloc (vm_addr vaddr, int type);
void KFree (void *addr);



#define MEM_TYPE_PAGETABLE  1
#define MEM_TYPE_PAGEDIR    2
#define MEM_TYPE_PAGE       3
#define MEM_TYPE_KERNEL     4




/*
 * VM Macros
 */

#define ALIGN_UP(val, alignment)                                \
    ((((val) + (alignment) - 1)/(alignment))*(alignment))
    
    
    
#define ALIGN_DOWN(val, alignment)                              \
            ((val) - ((val) % (alignment)))





#endif

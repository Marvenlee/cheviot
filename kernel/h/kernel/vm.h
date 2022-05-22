#ifndef KERNEL_VM_H
#define KERNEL_VM_H

#include <kernel/arch.h>
#include <kernel/lists.h>
#include <kernel/types.h>

/**
 *
 */

#define MEM_TYPE_PAGETABLE 1
#define MEM_TYPE_PAGEDIR 2
#define MEM_TYPE_PAGE 3
#define MEM_TYPE_KERNEL 4

/*
 * VirtualAlloc flags
 */

/*
 * VirtualAlloc() protections
 */
#define PROT_NONE			    0
#define PROT_READ			    (1<<0)
#define PROT_WRITE			    (1<<1)
#define PROT_EXEC			    (1<<2)
#define PROT_ALL			    (PROT_READ | PROT_WRITE | PROT_EXEC)
#define PROT_READWRITE 		    (PROT_READ | PROT_WRITE)
#define PROT_READEXEC		    (PROT_READ | PROT_EXEC)

#define MAP_FIXED				(1<<3)
#define MAP_WIRED				(1<<4)
#define MAP_NOX64				(1<<5)
#define MAP_BELOW16M			(1<<6)
#define MAP_BELOW4G				(1<<7)

#define CACHE_DEFAULT	 		(0<<8)
#define CACHE_WRITEBACK	 		(1<<8)
#define CACHE_WRITETHRU	 		(2<<8)
#define CACHE_WRITECOMBINE 		(3<<8)
#define CACHE_UNCACHEABLE  		(4<<8)
#define CACHE_WEAKUNCACHEABLE	(5<<8)
#define CACHE_WRITEPROTECT		(6<<8)


#define NCACHE_POLICIES 5

// Flags used for kernel administration of pages
#define MEM_RESERVED (0 << 28)
#define MEM_GARBAGE (1 << 28)
#define MEM_ALLOC (2 << 28)
#define MEM_PHYS (3 << 28)
#define MEM_FREE (4 << 28)

// FIXME: Rename MAP_COW and MAP_USER (off limits to user apps)
#define MAP_COW (1 << 26)
#define MAP_USER (1 << 27)

#define MEM_MASK 0xF0000000
#define PROT_MASK 0x0000000F
#define CACHE_MASK 0x00000F00

#define VM_SYSTEM_MASK (MEM_MASK | MAP_COW | MAP_USER)

#define MEM_TYPE(flags) (flags & MEM_MASK)

//#define CACHE_AGE               (1<<0)
//#define CACHE_BUSY				(1<<0)

/*
 *
 */

#define PGF_INUSE (1 << 0)
#define PGF_RESERVED (1 << 1)
#define PGF_CLEAR (1 << 2)
#define PGF_KERNEL (1 << 3)
#define PGF_USER (1 << 4)
#define PGF_PAGETABLE (1 << 5)

/*
 *
 */

#define NSEGMENT 32

#define SEG_TYPE_FREE 0
#define SEG_TYPE_ALLOC 1
#define SEG_TYPE_PHYS 2
#define SEG_TYPE_CEILING 3
#define SEG_TYPE_MASK 0x0000000f
#define SEG_ADDR_MASK 0xfffff000

struct Pageframe {
  vm_size size; // Pageframe is either 64k, 16k, or 4k
  vm_addr physical_addr;
  int reference_cnt; // Count of vpage references.
  bits32_t flags;
  LIST_ENTRY(Pageframe)
  link; // cache lru, busy link.  (busy and LRU on separate lists?)
  LIST_ENTRY(Pageframe)
  free_slab_link;
  struct PmapPageframe pmap_pageframe;
  size_t free_object_size;
  int free_object_cnt;
  void *free_object_list_head;
};

struct AddressSpace {
  struct Pmap pmap;

  vm_addr segment_table[NSEGMENT];
  int segment_cnt;
};

LIST_TYPE(Pageframe)
pageframe_list_t;

//#define NSLAB_SZ 3

#define SLAB_4K_SZ 0
#define SLAB_16K_SZ 1
#define SLAB_64K_SZ 2

// vm/addressspace.c

// int InitAddressSpace (struct AddressSpace *as);

int ForkAddressSpace(struct AddressSpace *new_as, struct AddressSpace *old_as);
void CleanupAddressSpace(struct AddressSpace *as);
void FreeAddressSpace(struct AddressSpace *as);

// arch/pmap.c

int PmapCreate(struct AddressSpace *as);
void PmapDestroy(struct AddressSpace *as);

int PmapSupportsCachePolicy(bits32_t flags);

int PmapEnter(struct AddressSpace *as, vm_addr addr, vm_addr paddr,
              bits32_t flags);
int PmapRemove(struct AddressSpace *as, vm_addr addr);
int PmapProtect(struct AddressSpace *as, vm_addr addr, bits32_t flags);

int PmapExtract(struct AddressSpace *as, vm_addr va, vm_addr *pa,
                uint32_t *flags);

// Could merge into PmapExtract, return a status flag, with -1 for no pte, -2
// for no pde etc.
bool PmapIsPageTablePresent(struct AddressSpace *as, vm_addr va);
bool PmapIsPagePresent(struct AddressSpace *as, vm_addr va);

void PmapFlushTLBs(void);
void PmapInvalidateAll(void);
void PmapSwitch(struct Process *next, struct Process *current);

void PmapPageframeInit(struct PmapPageframe *ppf);

vm_addr PmapPfToPa(struct Pageframe *pf);
struct Pageframe *PmapPaToPf(vm_addr pa);
vm_addr PmapVaToPa(vm_addr vaddr);
vm_addr PmapPaToVa(vm_addr paddr);
vm_addr PmapPfToVa(struct Pageframe *pf);
struct Pageframe *PmapVaToPf(vm_addr va);
int PmapCacheEnter(vm_addr addr, vm_addr paddr);
int PmapCacheRemove(vm_addr va);
int PmapCacheExtract(vm_addr va, vm_addr *pa);

// uint32_t *PmapAllocPageDirectory (void);

// vm/pagefault.c

int PageFault(vm_addr addr, bits32_t access);
void SysDoPageFault(struct Process *proc);

// vm/vm.c

SYSCALL void *SysVirtualAlloc(void *addr, size_t len, bits32_t flags);
SYSCALL void *SysVirtualAllocPhys(void *addr, size_t len, bits32_t flags, void *paddr);
SYSCALL int SysVirtualFree(void *addr, size_t size);
SYSCALL int SysVirtualProtect(void *addr, size_t size, bits32_t flags);

vm_addr SegmentCreate(struct AddressSpace *as, vm_offset addr, vm_size size,
                      int type, bits32_t flags);
void SegmentFree(struct AddressSpace *as, vm_addr base, vm_size size);
void SegmentInsert(struct AddressSpace *as, int index, int cnt);
vm_addr *SegmentFind(struct AddressSpace *as, vm_addr addr);
void SegmentCoalesce(struct AddressSpace *as);
vm_addr *SegmentAlloc(struct AddressSpace *as, vm_size size, uint32_t flags,
                      vm_addr *ret_addr);
int SegmentSplice(struct AddressSpace *as, vm_addr addr);

// arch/memcpy.s

int CopyIn(void *dst, const void *src, size_t sz);
int CopyOut(void *dst, const void *src, size_t sz);
int CopyInString(void *dst, const void *src, size_t max_sz);

void MemSet(void *mem, int c, size_t sz);
void MemCpy(void *dst, const void *src, size_t sz);

// pageframe.c

struct Pageframe *AllocPageframe(vm_size);
void FreePageframe(struct Pageframe *pf);
void CoalesceSlab(struct Pageframe *pf);
struct Pageframe *PhysicalAddrToPageframe(vm_addr pa);

void *AllocPage(void);
void FreePage(void *va);

/*
 * VM Macros
 */

#define ALIGN_UP(val, alignment)                                               \
  ((((val) + (alignment)-1) / (alignment)) * (alignment))

#define ALIGN_DOWN(val, alignment) ((val) - ((val) % (alignment)))

#endif

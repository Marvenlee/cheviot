#ifndef KERNEL_VM_H
#define KERNEL_VM_H

#include <sys/syscalls.h>
#include <kernel/arch.h>
#include <kernel/lists.h>
#include <kernel/types.h>

// Forward declarations
struct Pageframe;

// Linked list types
LIST_TYPE(Pageframe, pageframe_list_t, pageframe_list_link_t);


// Flags used for kernel administration of pages
#define MEM_RESERVED  (0 << 28)
#define MEM_GARBAGE   (1 << 28)
#define MEM_ALLOC     (2 << 28)
#define MEM_PHYS      (3 << 28)
#define MEM_FREE      (4 << 28)

#define MAP_COW       (1 << 26)
#define MAP_USER      (1 << 27)

// PROT_MASK and CACHE_MASK are defined in sys/syscalls.h
#define MEM_MASK 0xF0000000
#define VM_SYSTEM_MASK (MEM_MASK | MAP_COW | MAP_USER)


// Pageframe.flags
#define PGF_INUSE       (1 << 0)
#define PGF_RESERVED    (1 << 1)
#define PGF_CLEAR       (1 << 2)
#define PGF_KERNEL      (1 << 3)
#define PGF_USER        (1 << 4)
#define PGF_PAGETABLE   (1 << 5)

// Number of segments in an address space 
#define NSEGMENT 32

// Lower bits of AddressSpace.segment_table[]
#define SEG_TYPE_FREE 0
#define SEG_TYPE_ALLOC 1
#define SEG_TYPE_PHYS 2
#define SEG_TYPE_CEILING 3
#define SEG_TYPE_MASK 0x0000000f
#define SEG_ADDR_MASK 0xfffff000


/* @brief   Structure representing a physical page of memory
 */
struct Pageframe
{
  vm_size size;                           // Pageframe is either 64k, 16k, or 4k
  vm_addr physical_addr;
  int reference_cnt;                      // Count of vpage references.
  bits32_t flags;
  pageframe_list_link_t link;            // cache lru, busy link.  (busy and LRU on separate lists?)
  pageframe_list_link_t free_slab_link;
  struct PmapPageframe pmap_pageframe;
  size_t free_object_size;
  int free_object_cnt;
  void *free_object_list_head;
};


/* @brief   Address space of a process
 *
 * TODO: Convert segments back to list of memregions instead of small array
 * The original intent was to have a single address space OS with a single
 * segment table. This segment table could be quickly searched with a binary
 * tree (though possibly expensive to insert).
 */
struct AddressSpace
{
  struct Pmap pmap;
  vm_addr segment_table[NSEGMENT];
  int segment_cnt;
};


/*
 * Prototypes
 */

// vm/addressspace.c
int fork_address_space(struct AddressSpace *new_as, struct AddressSpace *old_as);
void cleanup_address_space(struct AddressSpace *as);
void free_address_space(struct AddressSpace *as);

// boards/.../pmap.c
int pmap_create(struct AddressSpace *as);
void pmap_destroy(struct AddressSpace *as);

int pmap_supports_cache_policy(bits32_t flags);

int pmap_enter(struct AddressSpace *as, vm_addr addr, vm_addr paddr, bits32_t flags);
int pmap_remove(struct AddressSpace *as, vm_addr addr);
int pmap_protect(struct AddressSpace *as, vm_addr addr, bits32_t flags);
int pmap_extract(struct AddressSpace *as, vm_addr va, vm_addr *pa, bits32_t *flags);

// Could merge into PmapExtract, return a status flag, with -1 for no pte, -2
// for no pde etc.
bool pmap_is_pagetable_present(struct AddressSpace *as, vm_addr va);
bool pmap_is_page_present(struct AddressSpace *as, vm_addr va);

uint32_t *pmap_alloc_pagetable(void);
void pmap_free_pagetable(uint32_t *pt);

void pmap_flush_tlbs(void);
void pmap_invalidate_all(void);
void pmap_switch(struct Process *next, struct Process *current);

void pmap_pageframe_init(struct PmapPageframe *ppf);

vm_addr pmap_pf_to_pa(struct Pageframe *pf);
struct Pageframe *pmap_pa_to_pf(vm_addr pa);
vm_addr pmap_va_to_pa(vm_addr vaddr);
vm_addr pmap_pa_to_va(vm_addr paddr);
vm_addr pmap_pf_to_va(struct Pageframe *pf);
struct Pageframe *pmap_va_to_pf(vm_addr va);

int pmap_cache_enter(vm_addr addr, vm_addr paddr);
int pmap_cache_remove(vm_addr va);
int pmap_cache_extract(vm_addr va, vm_addr *pa);

int pmap_interprocess_copy(struct AddressSpace *dst_as, void *dst, 
                           struct AddressSpace *src_as, void *src,
                           size_t sz);

// vm/pagefault.c
int page_fault(vm_addr addr, bits32_t access);

// vm/vm.c
void *sys_virtualalloc(void *addr, size_t len, bits32_t flags);
void *sys_virtualallocphys(void *addr, size_t len, bits32_t flags, void *paddr);
int sys_virtualfree(void *addr, size_t size);
int sys_virtualprotect(void *addr, size_t size, bits32_t flags);

vm_addr segment_create(struct AddressSpace *as, vm_offset addr, vm_size size,
                      int type, bits32_t flags);
void segment_free(struct AddressSpace *as, vm_addr base, vm_size size);
void segment_insert(struct AddressSpace *as, int index, int cnt);
vm_addr *segment_find(struct AddressSpace *as, vm_addr addr);
void segment_coalesce(struct AddressSpace *as);
vm_addr *segment_alloc(struct AddressSpace *as, vm_size size, uint32_t flags,
                      vm_addr *ret_addr);
int segment_splice(struct AddressSpace *as, vm_addr addr);

// arch/memcpy.s
int CopyIn(void *dst, const void *src, size_t sz);
int CopyOut(void *dst, const void *src, size_t sz);
int CopyInString(void *dst, const void *src, size_t max_sz);

// vm/page.c
void *kmalloc_page(void);
void kfree_page(void *vaddr);
struct Pageframe *alloc_pageframe(vm_size);
void free_pageframe(struct Pageframe *pf);
void coalesce_slab(struct Pageframe *pf);


/*
 * VM Macros
 * TODO: Replace ALIGN_UP and ALIGN_DOWN macros with roundup and rounddown
 * from <sys/param.h>
 */

#define ALIGN_UP(val, alignment)                                               \
  ((((val) + (alignment)-1) / (alignment)) * (alignment))

#define ALIGN_DOWN(val, alignment) ((val) - ((val) % (alignment)))

#endif

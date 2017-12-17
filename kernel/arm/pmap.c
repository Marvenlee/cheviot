/*
 * Copyright 2014  Marven Gilhespie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * ARM-Specific memory management code.  Deals with converting high-level
 * VirtSeg and PhysSeg segment tables into CPU page tables.
 */

#include <kernel/types.h>
#include <kernel/vm.h>
#include <kernel/arm/arm.h>
#include <kernel/dbg.h>
#include <kernel/utility.h>
#include <kernel/proc.h>
#include <kernel/error.h>
#include <kernel/globals.h>




uint32 *PmapAllocPageTable (void);
void PmapFreePageTable (uint32 *pt);

uint32 *PmapAllocPageDirectory (void);
void PmapFreePageDirectory (uint32 *pd);

int PmapExtract (struct AddressSpace *as, vm_addr va, vm_addr *pa, uint32 *flags);
bool PmapIsPageTablePresent(struct AddressSpace *as, vm_addr va);
bool PmapIsPagePresent(struct AddressSpace *as,vm_addr va);


int PmapPageTableIncRefCnt (uint32 *pt);
int PmapPageTableDecRefCnt (uint32 *pt);




int PmapInit (struct AddressSpace *as)
{
    struct Pmap *pmap;
    uint32 *pd;
    
    pmap = as->pmap;    
    pd = PmapAllocPageDirectory();
    
    if (pd == NULL)
    {
        return -1;
    }
    
    pmap->l1_table = pd;
    return 0;
}   





/*
 * Checks if the flags argument of VirtualAlloc() and VirtualAllocPhys() is
 * valid and supported.  Returns 0 on success, -1 otherwise.
 */

int PmapSupportsCachePolicy (bits32_t flags)
{
    return 0;
}



    



/*
 * Frees CPU dependent virtual memory management structures.
 */

void PmapDestroy (struct AddressSpace *as)
{
/*
    struct Pmap *pmap;
    
    pmap = proc->pmap;

    if (pmap != NULL)
    {
        LIST_ADD_TAIL (&pmap_lru_list, pmap, lru_link);
        proc->pmap = NULL;
    }
        
    PmapFlushTLBs();
*/
}



/*
*/

int PmapFork (struct AddressSpace *new_as, struct AddressSpace *old_as)
{
//    int pde_idx;
    vm_addr vpt, va, pa;
    bits32_t flags;
    struct Pageframe *pf;

    
    
    for (vpt = VM_USER_BASE; vpt <= VM_USER_CEILING; vpt += PAGE_SIZE * N_PAGETABLE_PTE)
    {
        if (PmapIsPageTablePresent(old_as, vpt))
        {
            for (va = vpt; va < vpt + PAGE_SIZE * N_PAGETABLE_PTE; va += PAGE_SIZE)
            {
                if (PmapIsPagePresent(old_as, va))
                {    
                    PmapExtract (old_as, va, &pa, &flags);
                    
                    if ((flags & MAP_PHYS) == 0)
                    {
                        pf = &pageframe_table[pa/PAGE_SIZE];
                        if (flags & PROT_WRITE)
                        {
                            // Mark page in both as read-only;                               
                            flags |= MAP_COW;                                
                            PmapEnter (old_as, va, pa, flags);                                
                            PmapEnter (new_as, va, pa, flags);
                        }
                        else
                        {
                            // Read-only, 
                            PmapEnter (new_as, va, pa, flags);                        
                        }
                            
                        pf->reference_cnt ++;
                    }
                    else 
                    {
                        // Physical Mapping
                        PmapEnter (new_as, va, pa, flags);                        
                    }                    
                }
            }
        }
    }
    
	return 0;
}


int PmapKEnter (vm_addr va, vm_addr pa, bits32_t flags)
{
    return -1;
}


/*
 * FIXME:  Do allocation of ANON pages here ????????
 * Matches freeing we do of existing page mapping
 *
 *
 * 4k page tables,  1k real PTEs,  3k virtual-page linked list and flags (packed 12 bytes)
 */

int PmapEnter (struct AddressSpace *as, vm_addr addr, vm_addr paddr, bits32_t flags)
{
	struct Pmap *pmap;
    uint32 *pt, *phys_pt;
    int pde_idx, pte_idx;
    uint32 pa_bits;
//    bool present;
    vm_addr current_paddr;
    struct Pageframe *pf;
    uint32 vpte_flags; 
    struct PmapVPTE *vpte;
    struct PmapVPTE *vpte_base;
    vm_addr pa;

    pmap = as->pmap;

    pa_bits = L2_TYPE_S;
    
    if ((flags & PROT_WRITE) == PROT_WRITE)
        pa_bits = L2_AP(AP_U | AP_W);
    else
        pa_bits = L2_AP(AP_U);
     
    if ((flags & MEM_MASK) == MEM_ALLOC)
        pa_bits |= L2_B | L2_C;

    
    // Could just copy the VM interface flags here
    // Ensure some are available for system/pmap use.
    vpte_flags= 0;
    vpte_flags |= (flags & MAP_PHYS) ? VPTE_PHYS: 0;
    vpte_flags |= (flags & MAP_LAZY) ? VPTE_LAZY: 0;
    vpte_flags |= (flags & MAP_COW) ? VPTE_PROT_COW : 0;
    vpte_flags |= (flags & PROT_READ) ? VPTE_PROT_R: 0;
    vpte_flags |= (flags & PROT_WRITE) ? VPTE_PROT_W: 0;
    vpte_flags |= (flags & PROT_EXEC) ? VPTE_PROT_X: 0;
    vpte_flags |= (flags & MAP_WIRED) ? VPTE_WIRED: 0;

        
    pde_idx = (addr & L1_ADDR_BITS) >> L1_IDX_SHIFT;

    // FIXME:  Need a GetPagedir
    
    if ((pmap->l1_table[pde_idx] & L1_TYPE_MASK) == L1_TYPE_INV)
    {
		if ((pt = PmapAllocPageTable()) == NULL)
		{
			return memoryErr;
		}

        phys_pt = (uint32 *)PmapVaToPa ((vm_addr)pt);

        pmap->l1_table[pde_idx] = (uint32)phys_pt | L1_TYPE_C;
    }
    else
    {
        // Need a GetPagedir (after releasing pagedir)
    
        phys_pt = (uint32 *) (pmap->l1_table[pde_idx] & L1_C_ADDR_MASK);
        
        pt = (uint32 *)PmapPaToVa ((vm_addr)phys_pt);
    }
    
    pte_idx = (addr & L2_ADDR_BITS) >> L2_IDX_SHIFT;
    
    
    current_paddr = pt[pte_idx] & L2_ADDR_BITS;
  
    vpte_base = (struct PmapVPTE *)((uint8 *)pt + VPTE_TABLE_OFFS);
    vpte = vpte_base + pte_idx;

  
    
    // FIXME: ?? Do we need to check for COW here ???????????
    // pa_bits needs to be read-only if COW set.

    if ((pt[pte_idx] & L2_TYPE_MASK) != L2_TYPE_INV && current_paddr == paddr)
    {
        // Change protections

        pt[pte_idx] = paddr | pa_bits;        
        vpte->flags = vpte_flags;    
    }
    else
    {
        // Unmap any existing mapping, then map either anon or phys mem
        if ((pt[pte_idx] & L2_TYPE_MASK) != L2_TYPE_INV)
        {   
            if  ((vpte->flags & VPTE_PHYS) == 0)
            {            
                pf = PmapPaToPf (current_paddr);
                
                LIST_REM_ENTRY (&pf->pmap_pageframe.vpte_list, vpte, link);
                pf->reference_cnt --;
                
                if (pf->reference_cnt == 0)
                {
                    FreePageframe (pf);
                }
            }

            pt[pte_idx] = L2_TYPE_INV;
            PmapPageTableDecRefCnt (pt);    // Could potentially free pagetable/optimize ?
        }
        
        if ((flags & VPTE_PHYS) == 0)
        {
            pf = AllocPageframe (PAGE_SIZE);
            
            if (pf == NULL)
                return memoryErr;
/* FIXME
            if (pf->dirty)
            {
                MemSet();
            }
 */
          
            pa = PmapPfToPa (pf);     
            vpte->flags = vpte_flags;
            
            LIST_ADD_HEAD (&pf->pmap_pageframe.vpte_list, vpte, link);
            pf->reference_cnt ++;
            pt[pte_idx] = pa | pa_bits;
            PmapPageTableIncRefCnt (pt);    
            
        }
        else
        {
            vpte->flags = vpte_flags;
            pt[pte_idx] = paddr | pa_bits;                        
            PmapPageTableDecRefCnt (pt);
        }
    }    
            
    return 0;
}


/*
 * Unmaps a segment from the address space pointed to by pmap. 
 */

int PmapRemove (struct AddressSpace *as, vm_addr va)
{
	struct Pmap *pmap;
    uint32 *pt, *phys_pt;
    int pde_idx, pte_idx;
//    uint32 pa_bits;
//    bool present;
    vm_addr current_paddr;
    struct Pageframe *pf;
//    uint32 vpte_flags; 
    struct PmapVPTE *vpte;
    struct PmapVPTE *vpte_base;
//    vm_addr pa;

    pmap = as->pmap;
    pde_idx = (va & L1_ADDR_BITS) >> L1_IDX_SHIFT;
    
    if ((pmap->l1_table[pde_idx] & L1_TYPE_MASK) == L1_TYPE_INV)
    {
        return memoryErr;
    }
    else
    {
        phys_pt = (uint32 *) (pmap->l1_table[pde_idx] & L1_C_ADDR_MASK);
        pt = (uint32 *)PmapPaToVa ((vm_addr)phys_pt);

        pte_idx = (va & L2_ADDR_BITS) >> L2_IDX_SHIFT;
        current_paddr = pt[pte_idx] & L2_ADDR_BITS;

        vpte_base = (struct PmapVPTE *)((uint8 *)pt + VPTE_TABLE_OFFS);
        vpte = vpte_base + pte_idx;

        // Unmap any existing mapping, then map either anon or phys mem
        if ((pt[pte_idx] & L2_TYPE_MASK) != L2_TYPE_INV)
        {   
            if  ((vpte->flags & VPTE_PHYS) == 0)
            {
                pf = PmapPaToPf (current_paddr);
                
                LIST_REM_ENTRY (&pf->pmap_pageframe.vpte_list, vpte, link);
                pf->reference_cnt --;
                
                if (pf->reference_cnt == 0)
                {
                    FreePageframe (pf);
                }
            }

            pt[pte_idx] = 0;
            PmapPageTableDecRefCnt (pt);
        }
    
        // decrement pt reference count,  check if pagetable needs freeing.
        
        if (PmapPageTableDecRefCnt (pt) == 0)
        {
            PmapFreePageTable (pt);
        }    
        
        PmapFlushTLBs();
    }
        
    return 0;
}



int PmapRemoveAll (struct AddressSpace *as)
{
    return 0;
}



int PmapProtect (struct AddressSpace *as, vm_addr addr, bits32_t flags)
{
    return 0;
}

void PmapPageframeInit (struct PmapPageframe *ppf)
{
    LIST_INIT (&ppf->vpte_list);
}


/*
*/

int PmapExtract (struct AddressSpace *as, vm_addr va, vm_addr *pa, uint32 *flags)
{
    // TODO: PmapExtract 

    *pa = 0;
    *flags = 0;
	return memoryErr;
}


bool PmapIsPageTablePresent(struct AddressSpace *as, vm_addr addr)
{
	struct Pmap *pmap;
    int pde_idx;
    
    pmap = as->pmap;

    pde_idx = (addr & L1_ADDR_BITS) >> L1_IDX_SHIFT;
    
    if ((pmap->l1_table[pde_idx] & L1_TYPE_MASK) == L1_TYPE_INV)
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }    
}



bool PmapIsPagePresent(struct AddressSpace *as, vm_addr addr)
{
    struct Pmap *pmap;
    uint32 *pt, *phys_pt;
    int pde_idx, pte_idx;
//    vm_addr pa;

    pmap = as->pmap;
    pde_idx = (addr & L1_ADDR_BITS) >> L1_IDX_SHIFT;
    
    if ((pmap->l1_table[pde_idx] & L1_TYPE_MASK) == L1_TYPE_INV)
    {
        return FALSE;
    }
    else
    {
        phys_pt = (uint32 *) (pmap->l1_table[pde_idx] & L1_C_ADDR_MASK);
        pt = (uint32 *)PmapPaToVa ((vm_addr)phys_pt);
    
        pte_idx = (addr & L2_ADDR_BITS) >> L2_IDX_SHIFT;

        if ((pt[pte_idx] & L2_TYPE_MASK) != L2_TYPE_INV)
            return TRUE;
        else
            return FALSE;
    }
}




/*
*/

uint32 *PmapAllocPageTable (void)
{
	int t;
	uint32 *pt;
	struct Pageframe *pf;
	
	if ((pf = AllocPageframe (VPAGETABLE_SZ)) == NULL)
	    return NULL;
	
    pt = (uint32 *)PmapPfToVa (pf);
	
    for (t=0; t<256; t++)
    {
        *(pt + t) = L2_TYPE_INV;
	}

	return pt;
}


/*
*/

void PmapFreePageTable (uint32 *pt)
{
    struct Pageframe *pf;
    pf = PmapVaToPf ((vm_addr)pt);
    FreePageframe (pf);
}


int PmapPageTableIncRefCnt (uint32 *pt)
{
    struct Pageframe *pf;
    pf = PmapVaToPf ((vm_addr)pt);
    pf->reference_cnt ++;
    
    return pf->reference_cnt;
}

int PmapPageTableDecRefCnt (uint32 *pt)
{
    struct Pageframe *pf;
    pf = PmapVaToPf ((vm_addr)pt);
    pf->reference_cnt --;
    
    return pf->reference_cnt;
}


/*
*/

uint32 *PmapAllocPageDirectory (void)
{
	uint32 *pd;
	int t;
    struct Pageframe *pf;
    
	if ((pf = AllocPageframe (PAGEDIR_SZ)) == NULL)
	    return NULL;
	    
    pd = (uint32 *)PmapPfToVa (pf);	
	
    for (t=0; t<4096; t++)
    {
        *(pd + t) = L1_TYPE_INV;
	}

	return pd;
}


/*
*/

void PmapFreePageDirectory (uint32 *pd)
{
    struct Pageframe *pf;    
    pf = PmapVaToPf ((vm_addr)pd);
    FreePageframe (pf);
}



vm_addr PmapPfToPa (struct Pageframe *pf)
{
    return (vm_addr)pf->physical_addr;
}

struct Pageframe *PmapPaToPf (vm_addr pa)
{    
    return &pageframe_table[(vm_addr)pa / PAGE_SIZE];
}


vm_addr PmapPfToVa (struct Pageframe *pf)
{
    return PmapPaToVa ((pf->physical_addr));
}

struct Pageframe *PmapVaToPf (vm_addr va)
{
    return &pageframe_table[PmapVaToPa(va) / PAGE_SIZE];
}




vm_addr PmapVaToPa (vm_addr vaddr)
{
    return vaddr - 0x80000000;    
}




vm_addr PmapPaToVa (vm_addr paddr)
{
    return paddr + 0x80000000;    
}



/*
 * Flushes the CPU's TLBs once page tables are updated.
 */

void PmapFlushTLBs (void)
{
    //FIXME:  Could instead set a flag to InvalidateTLB on kernel exit or later point.
    InvalidateTLB();
}



void PmapBarrier (void)
{
    // ensure page table entries are written out (to RAM if page table walk doesn't look in L1)
}




/*
 * Switches the address space from the current process to the next process.
 */

void PmapSwitch (struct Process *next, struct Process *current)
{
// FIXME:   reload page directory 
}




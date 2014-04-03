#include "types.h"
#include "elf.h"
#include "fat.h"
#include "globals.h"
#include "memory.h"
#include "dbg.h"

/*
 * elf_load();
 */

int elf_load (char *filename, void **entry_point)
{
	int t;
	Elf32_PHdr *phdr;
	off_t phdr_offs, sec_offs;
	uint32 sec_addr;
	int32 sec_file_sz;
	uint32 sec_mem_sz;
	uint32 sec_prot;
	void *addr;
	
	KLOG ("elf_load()");
	
	if (fat_open (filename) == -1)
	{
		KLOG ("could not open file");
		return -1;
	}
	
	if (fat_read (&ehdr, sizeof (Elf32_EHdr)) != sizeof (Elf32_EHdr))
	{
		KLOG ("could not read elf header");
		return -1;
	}
	
	if (!(ehdr.e_ident[EI_MAG0] == ELFMAG0 && 
		ehdr.e_ident[EI_MAG1] == 'E' && 
		ehdr.e_ident[EI_MAG2] == 'L' && 
		ehdr.e_ident[EI_MAG3] == 'F' && 
		ehdr.e_ident[EI_CLASS] == ELFCLASS32 &&
		ehdr.e_ident[EI_DATA] == ELFDATA2LSB &&
		ehdr.e_machine == EM_ARM &&
		ehdr.e_type == ET_EXEC &&
		ehdr.e_phnum > 0))
	{
		KLOG ("Incompatible ELF or not ELF");
		return -1;	
	}
	
	phdr_cnt = ehdr.e_phnum;
	phdr_offs = ehdr.e_phoff;
	*entry_point = (void (*)(int , char *[], int , char *[],
							int , char *[])) (ehdr.e_entry);
	
	if (phdr_cnt > MAX_PHDR)
		return -1;
	
	fat_seek (phdr_offs);

	if (fat_read (phdr_table, sizeof (Elf32_PHdr) * phdr_cnt) != sizeof (Elf32_PHdr) * phdr_cnt)
		return -1;
	
	for (t=0; t < phdr_cnt; t++)
	{
		KLOG ("phdr = %d", t);
		
		phdr = phdr_table+t;
		
		if (phdr->p_type != PT_LOAD)
		{
			continue;
		}
		
		sec_addr = phdr->p_vaddr;
		sec_file_sz = phdr->p_filesz;
		sec_mem_sz = phdr->p_memsz;
		sec_offs = phdr->p_offset;
				
		if (sec_mem_sz < sec_file_sz)
			return -1;

		sec_addr = ALIGN_DOWN (phdr->p_vaddr, phdr->p_align);
		sec_mem_sz = phdr->p_memsz;
		sec_prot = 0;
		
		if (phdr->p_flags & PF_X)
			sec_prot |= PROT_EXEC;
		if (phdr->p_flags & PF_R)
			sec_prot |= PROT_READ;
		if (phdr->p_flags & PF_W)
			sec_prot |= PROT_WRITE;
		
		addr = SegmentCreate (sec_addr, sec_mem_sz, SEG_TYPE_ALLOC, MAP_FIXED | sec_prot);

		KLog ("%#010x = SegmentCreate (%#010x, %d", addr, sec_addr, sec_mem_sz);
					
		if (addr != (void *)sec_addr)
		{
			KPanic ("Segment address does not match fixed address");
		}
		
		KLOG ("p_vaddr = %#010x, sz = %d", phdr->p_vaddr, sec_mem_sz);
				
		if (sec_file_sz != 0)
		{
			fat_seek (sec_offs);

			if (fat_read ((void *)phdr->p_vaddr, sec_file_sz) != sec_file_sz)
				return -1;
		}
	}
	
	return 0;
}


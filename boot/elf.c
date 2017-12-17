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
	off_t phdr_offs;
	
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
		    KLOG ("Not PT_LOAD");
			continue;
		}

		if (phdr->p_memsz < phdr->p_filesz)
		{
		    KLOG("ELF corrupt, mem_sz < file_sz");
			return -1;
        }
        
		if (phdr->p_filesz != 0)
		{
		    KLOG ("Reading in elf section");
		    
			fat_seek (phdr->p_offset);

			if (fat_read ((void *)phdr->p_paddr, phdr->p_filesz) != phdr->p_filesz)
			{
			    KLOG ("FAT read failed");
				return -1;
		    }
		}
	}

    KLOG ("elf_load() OK");
	
	return 0;
}


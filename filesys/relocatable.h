/*
 * Elf32 Relocatable object loader.
 * Useful for loading modules into the kernel or applications in a single address
 * space OS.
 */


#include <sys/types.h>
#include <sys/syscalls.h>
#include "elf.h"




/*
 * Prototypes and Non-ELF structures
 */

Elf32_EHdr *Elf32ModuleLoad (int fd);
int Elf32IsRelocatable (Elf32_EHdr *ehdr);
int Elf32SimplifySymbols (Elf32_EHdr *ehdr);
int Elf32Relocate (Elf32_EHdr *ehdr);
int Elf32BoundsCheck (uint32 base, uint32 ceiling, uint32 addr, uint32 sz);
int Elf32Protect (Elf32_EHdr *elf);
Elf32_Sym *Elf32FindSymbol (char *name, Elf32_EHdr *ehdr);


struct KSym
{
	char *name;
	void *addr;
};

struct KSym *Elf32ImportSymbol (char *name);





/*
 * LoadModule();
 *
 * Example of loading a module and calling a function named "__entry".
 */

Elf32_EHdr *LoadModule (char *name)
{
	int fd;
	Elf32_EHdr *ehdr;
	void (entry_point *) (void);
	Elf32_Sym *sym;
	
	
	if ((fd = open (name, O_RDONLY, 0)) != -1)
	{
		if (Elf32LoadModule (fd, &ehdr) == 0)
		{
			if (Elf32SimplifySymbols (ehdr) == 0)
			{
				if (Elf32Relocate (ehdr) == 0)
				{
					if (Elf32Protect (ehdr) == 0)
					{
						sym = Elf32FindSymbol ("__entry", ehdr);
						entry_point = sym->st_value;
						entry_point();

						close (fd);
						return ehdr;
					}
				}
			}
		}
		
		close (fd);
	}
	
	return NULL;
}





/*
 * Elf32ModuleLoad();
 *
 * Loads Elf Header, section table and sections into memory.
 * We load the Elf structures into memory,  then modify fields
 * to point to their in-memory locations.
 * 
 * ehdr->e_shoff = in-memory address of section header table.
 * shdr->sh_addr = in-memory address of section.
 * 
 * NOTE:  Needs additional error checking and cleanup.
 */

int Elf32ModuleLoad (int fd, Elf32_EHdr **r_ehdr)
{
	Elf32_EHdr *ehdr;
	void *shdr_table;
	Elf32_SHdr *shdr;
	int32 t;

		
	/* Load and validate Elf header */

	ehdr = (Elf32_EHdr *) kmalloc (sizeof (Elf32_EHdr));
	*r_ehdr = ehdr;
		
	lseek (fd, 0, SEEK_SET);
	read (fd, ehdr, sizeof (Elf32_EHdr));
		
	if (Elf32IsRelocatable (ehdr) == -1)
		return -1;
	
	
	/* Allocate and load Section-Header table */	

	shdr_table = malloc (ehdr->e_shnum * ehdr->e_shentsize);

	lseek (fd, ehdr->e_shoff, SEEK_SET);
	read (fd, (void *) shdr_table, ehdr->e_shnum * ehdr->e_shentsize);

	ehdr->e_shoff = (Elf32_Off)shdr_table;
	
	
	
	for (t=1; t < ehdr->e_shnum; t++)
	{
		shdr = (Elf32_SHdr *)(ehdr->e_shoff + (t * ehdr->e_shentsize));
				
		if (shdr->sh_type == SHT_PROGBITS)
		{
			if ((shdr->sh_flags & SHF_ALLOC) == SHF_ALLOC)
			{
				if (shdr->sh_size == 0)
					continue;
			
				if ((shdr->sh_addr = mmap (shdr->sh_size, VM_PROT_ALL)) == MAP_FAILED)
					return -1;
				
				lseek (fd, shdr->sh_offset, SEEK_SET);
				read (fd, (void *) shdr->sh_addr, shdr->sh_size);

			}
			else
			{
				shdr->sh_addr = 0;
			}
			
		}
		else if (shdr->sh_type == SHT_NOBITS)
		{
			if (shdr->sh_flags & SHF_ALLOC)
			{
				shdr->sh_addr = 0;
				if (shdr->sh_size == 0)
					continue;
			
				if ((shdr->sh_addr = mmap (shdr->sh_size, VM_PROT_ALL)) == MAP_FAILED)
					return -1;
			}
			else
			{
				shdr->sh_addr = 0;
			}
		}
		else
		{
			if (shdr->sh_size == 0)
				continue;
			
			if ((shdr->sh_addr = kmap (shdr->sh_size, VM_PROT_ALL)) == MAP_FAILED)
				return -1;
			
			lseek (fd, shdr->sh_offset, SEEK_SET);
			read (fd, (void *) shdr->sh_addr, shdr->sh_size);
		}
	}

	return 0;
}




/*
 * Elf32IsRelocatable();
 *
 * Check that the file is ELF, relocatable (ET_REL) and i386 (EM_386).
 */

int Elf32IsRelocatable (Elf32_EHdr *ehdr)
{
	if (ehdr->e_ident[EI_MAG0] == ELFMAG0 && 
		ehdr->e_ident[EI_MAG1] == 'E' && 
		ehdr->e_ident[EI_MAG2] == 'L' && 
		ehdr->e_ident[EI_MAG3] == 'F' && 
		ehdr->e_ident[EI_CLASS] == ELFCLASS32 &&
		ehdr->e_ident[EI_DATA] == ELFDATA2LSB &&
		ehdr->e_machine == EM_386 &&
		ehdr->e_type == ET_REL &&
		ehdr->e_shnum > 0)
		return 0;
	else
		return -1;
}




/*
 * ElfSimplifySymbols();
 *
 * 1) Find String table
 * 
 * 2) Change value of symbols so that sh_value contains the real pointer 
 *	* Undefined symbols -  import values from external source, e.g. kernel
 *    symbol table
 *	* Absolute symbols - do nothing
 *	* Common symbols - kernel modules shouldn't have any, error condition.
 *	* All other symbols - add the symbol value to the address in memory of
 *    the section that holds it
 */

int Elf32SimplifySymbols (Elf32_EHdr *ehdr)
{
	uint32 shdr_table;
	Elf32_SHdr *shdr, *sym_shdr;
	int32 t, s;
	int32 max_sym;
	uint32 sym_table = 0;
	Elf32_Sym *sym;
	uint32 string_table = 0;
	uint32 sym_name;
	struct KSym *ksym;
	
	
	
	shdr_table = ehdr->e_shoff;
	
	for (t=0; t < ehdr->e_shnum; t++)
	{
		shdr = (Elf32_SHdr *)(shdr_table + (t * ehdr->e_shentsize));
		
		if (shdr->sh_type == SHT_STRTAB)
		{
			if (ehdr->e_shstrndx)
			{
				string_table = shdr->sh_addr;
			}
		}
	}
	
	if (string_table == 0)
		return -1;
	
	
	for (t = 1; t < ehdr->e_shnum; t++)
	{
		shdr = (Elf32_SHdr *)(shdr_table + (t * ehdr->e_shentsize));
		
		if (shdr->sh_type == SHT_SYMTAB)
		{
			sym_table = shdr->sh_addr;
			max_sym = shdr->sh_size / shdr->sh_entsize;
			
			for (s=1; s < max_sym; s++)
			{
				sym = (Elf32_Sym *)(sym_table + (s * shdr->sh_entsize));
				
				switch (sym->st_shndx)
				{
					case SHN_UNDEF:
						sym_name = string_table + sym->st_name;

						if ((ksym = Elf32ImportKernelSymbol ((char *)sym_name)) != NULL)
							sym->st_value = (uint32)ksym->addr;
						else
							return -1;
					
					case SHN_ABS:
						break;

					case SHN_COMMON:
						return -1;
				
					default:
						sym_shdr = (Elf32_SHdr *)(shdr_table + sym->st_shndx * ehdr->e_shentsize);
											
						if (sym_shdr->sh_addr != 0)
							sym->st_value = sym_shdr->sh_addr + sym->st_value;
				}
			}
		}
	}
	
	return 0;
}



	
/*
 * ELFRelocate();
 *
 * Search for SHT_REL relocation sections and perform relocations.
 * Performs bounds checks to ensure that the relocations are within sections
 */
	
int Elf32Relocate (Elf32_EHdr *ehdr)
{
	uint32 shdr_table;
	Elf32_SHdr *shdr, *symtable_shdr, *section_shdr;
	int32 t, s;
	int32 sym_num, rel_num;
	int32 sym_idx, rel_type;
	uint32 sym_table, rel_table;
	Elf32_Sym *sym;
	Elf32_Rel *rel;
	uint32 loc_r32, loc_pc32;
	uint32 section_base, section_ceiling;
	
	
		
	shdr_table = ehdr->e_shoff;
	
	for (t=1; t < ehdr->e_shnum; t++)
	{
		shdr = (Elf32_SHdr *)(shdr_table + (t * ehdr->e_shentsize));
		
		
		if (shdr->sh_type == SHT_REL)
		{
			rel_table = shdr->sh_addr;
			rel_num = shdr->sh_size / shdr->sh_entsize;
			

			if (shdr->sh_link > ehdr->e_shnum || shdr->sh_info > ehdr->e_shnum)
				return -1;
			
			
			
			symtable_shdr = (Elf32_SHdr *)(shdr_table
							+ (shdr->sh_link * ehdr->e_shentsize));
							
			sym_table = symtable_shdr->sh_addr;
			sym_num = symtable_shdr->sh_size / symtable_shdr->sh_entsize;
			
			section_shdr = (Elf32_SHdr *)(shdr_table
							+ (shdr->sh_info * ehdr->e_shentsize));
			
			
			
			if ((section_shdr->sh_flags & SHF_ALLOC)
					&& section_shdr->sh_addr != 0)
			{
				section_base = section_shdr->sh_addr;
				section_ceiling = section_base + section_shdr->sh_size;
			
				for (s=0; s < rel_num; s++)
				{
					rel = (Elf32_Rel *)(rel_table + (s * shdr->sh_entsize));
					
					sym_idx = Elf32_R_SYM (rel->r_info);
					rel_type = Elf32_R_TYPE (rel->r_info);
					
					sym = (Elf32_Sym *)(sym_table + (sym_idx * symtable_shdr->sh_entsize));
										
					if (sym_idx > sym_num)
						return -1;
					
					
					
					switch (rel_type)
					{
						case R_386_NONE:
							break;
							
						case R_386_32:
							loc_r32 = section_base + rel->r_offset;
							
							if (Elf32BoundsCheck (section_base, section_ceiling, loc_r32, 4) != 0)
								return -1;
							
							*(uint32 *)loc_r32 = *(uint32 *)loc_r32 + sym->st_value;
							break;
							
						case R_386_PC32:
							loc_pc32 = section_base + rel->r_offset;
							
							if (Elf32BoundsCheck (section_base, section_ceiling, loc_pc32, 4) != 0)
								return -1;
							
							*(uint32 *)loc_pc32 = *(uint32 *)loc_pc32
										+ sym->st_value - (Elf32_Word)loc_pc32;
							break;
							
						default:
							return -1;
					}
				}
			}
		}
	}
	
	return 0;
}





/*
 * ElfBoundsCheck();
 *
 * Ensures that updating the value of a variable is within the bounds of
 * a section.
 */
 
int Elf32BoundsCheck (uint32 base, uint32 ceiling, uint32 addr, uint32 sz)
{
	if (addr >= base && (addr + sz) <= ceiling)
		return 0;
	else
		return -1;
}




/*
 * Elf32Protect();
 * 
 * Update the protection modes of allocated sections.
 */

int Elf32Protect (Elf32_EHdr *ehdr)
{
	uint32 shdr_table;
	Elf32_SHdr *shdr;
	
	shdr_table = ehdr->e_shoff;


	for (t=1; t < ehdr->e_shnum; t++)
	{
		shdr = (Elf32_SHdr *)(shdr_table + (t * ehdr->e_shentsize));
		
		if ((shdr->sh_flags & SHF_ALLOC)
			&& shdr->sh_addr != 0)
		{
			prot = PROT_READ;
			
			if (shdr->sh_flags & SHF_SHF_WRITE)
				prot |= PROT_WRITE;
			
			if (shdr->sh_flags & SHF_SHF_WRITE)
				prot |= PROT_EXECUTE;

			mprotect (shdr->sh_addr, ALIGN_UP(shdr->sh_size, PAGE_SIZE), prot);
		}

	return 0;
}




/*
 * Elf32FindSymbol();
 *
 * Find a symbol in the module.
 */

Elf32_Sym *Elf32FindSymbol (char *name, Elf32_EHdr *ehdr)
{
	uint32 shdr_table;
	Elf32_SHdr *shdr;
	int32 t, s;
	
	int32 sym_num;
	
	uint32 sym_table = 0;
	uint32 string_table = 0;
	uint32 sym_name;
	Elf32_Sym *sym;

	
	shdr_table = ehdr->e_shoff;
		
	for (t = 1; t < ehdr->e_shnum; t++)
	{
		shdr = (Elf32_SHdr *)(shdr_table + (t * ehdr->e_shentsize));
				
		if (shdr->sh_type == SHT_STRTAB)
		{
			if (ehdr->e_shstrndx)
			{
				string_table = shdr->sh_addr;
			}
		}
	}
	
	if (string_table == 0)
		return NULL;
			
	
		
	for (t=1; t < ehdr->e_shnum; t++)
	{
		shdr = (Elf32_SHdr *)(shdr_table + (t * ehdr->e_shentsize));
		
		if (shdr->sh_type == SHT_SYMTAB)
		{
			sym_table = shdr->sh_addr;
			sym_num = shdr->sh_size / shdr->sh_entsize;
			
			for (s=1; s < sym_num; s++)
			{
				sym = (Elf32_Sym *)(sym_table + (s * shdr->sh_entsize));
				
				sym_name = string_table + sym->st_name;
				
				if (strcmp ((char *)sym_name, name) == 0)
				{
					return sym;
				}
			}
		}
	}
	
	return NULL;
}





/*
 *
 */

struct KSym kernel_symbol_table[] =
{
	{"fork",		&fork},
	{"exec",		&exec},		
	{"exit",		&exit},		
	
	/* etc, etc */
	
	{NULL, NULL}
};




/*
 * El32ImportSymbol();
 */

struct KSym *Elf32ImportSymbol (char *name)
{
	struct KSym *k;
	
	
	k = kernel_symbol_table;
	
	while (k->name != NULL)
	{
		if (strcmp (k->name, name) == 0)
			return k;

		k++;
	}	
	
	return NULL;
}


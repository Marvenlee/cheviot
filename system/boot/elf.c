#include "elf.h"
#include "dbg.h"
#include "globals.h"
#include "memory.h"
#include "sys/syscalls.h"
#include "types.h"
#include <stdint.h>
#include <string.h>


struct IFSHeader {
  char magic[4];
  uint32_t node_table_offset;
  int32_t node_cnt;
  uint32_t ifs_size;
} __attribute__((packed));

struct IFSNode {
  char name[32];
  int32_t inode_nr;
  int32_t parent_inode_nr;
  uint32_t permissions;
  int32_t uid;
  int32_t gid;
  uint32_t file_offset;
  uint32_t file_size;
} __attribute__((packed));


static void elf_seek(off_t offset);
static ssize_t elf_read(void *buf, size_t sz);
//int StrCmp(char *s1, char *s2);

static uint8_t *elf_base = NULL;
static size_t elf_offset = 0;
static size_t elf_size = 0;


int elf_find(void *ifs_image, size_t ifs_size, char *dirname, char *filename,  vm_addr *file, vm_size *file_size)
{
  int32_t boot_inode_nr;
  struct IFSHeader *ifs_header;
  struct IFSNode *ifs_node;
  bool boot_dir_found = FALSE;
  bool file_found = FALSE;

  ifs_header = (struct IFSHeader *)ifs_image;
  ifs_node = (struct IFSNode *)((uint8_t *)ifs_image + ifs_header->node_table_offset);

  for (int t = 0; t < ifs_header->node_cnt; t++) {
    KLog("node: %s", ifs_node->name);
    if (StrCmp(ifs_node->name, dirname) == 0) {
      boot_dir_found = TRUE;
      boot_inode_nr = ifs_node->inode_nr;
      break;
    }
    ifs_node++;
  }

  if (boot_dir_found == FALSE) {

    KLog("boot dir not found");
    return -1;
  }

  KLog("boot inode nr = %d", boot_inode_nr);

  ifs_node = (struct IFSNode *)((uint8_t *)ifs_image + ifs_header->node_table_offset);

  for (int t = 0; t < ifs_header->node_cnt; t++) {
    KLog("node: %s inode_nr = %d parent %d", ifs_node->name, ifs_node->inode_nr,
         ifs_node->parent_inode_nr);
    if (StrCmp(ifs_node->name, filename) == 0 &&
        ifs_node->parent_inode_nr == boot_inode_nr) {
      file_found = TRUE;
      break;
    }
    ifs_node++;
  }

  if (file_found == FALSE) {
    KLog("file not found");
    return -1;
  }

  KLog ("ifs node->offset = %d", ifs_node->file_offset);

  *file = (vm_addr)ifs_image + ifs_node->file_offset;
  *file_size = ifs_node->file_size;
}



/**
 *
 */
int elf_load(void *ifs_image, size_t ifs_size, char *filename, Elf32_EHdr *ehdr, Elf32_PHdr *phdr_table, vm_addr *ceiling)
{
  int t;
  Elf32_PHdr *phdr;
  off_t phdr_offs;
  vm_addr segment_base;
  vm_addr segment_ceiling;
  int32_t boot_inode_nr;
  struct IFSHeader *ifs_header;
  struct IFSNode *ifs_node;

  bool boot_dir_found = FALSE;
  bool file_found = FALSE;


  *ceiling = 0x00000000;
  
  ifs_header = (struct IFSHeader *)ifs_image;
  ifs_node = (struct IFSNode *)((uint8_t *)ifs_image + ifs_header->node_table_offset);

  KLog("ifs_header %c %c %c %c", ifs_header->magic[0], ifs_header->magic[1],
       ifs_header->magic[2], ifs_header->magic[3]);

  KLog("node_cnt = %d", ifs_header->node_cnt);
  
  for (t = 0; t < ifs_header->node_cnt; t++) {
    KLog("node: %s", ifs_node->name);
    if (StrCmp(ifs_node->name, "boot") == 0) {
      boot_dir_found = TRUE;
      boot_inode_nr = ifs_node->inode_nr;
      break;
    }
    ifs_node++;
  }

  if (boot_dir_found == FALSE) {
    KLog("boot dir not found");
    return -1;
  }


  KLog("boot inode nr = %d", boot_inode_nr);

  ifs_node = (struct IFSNode *)((uint8_t *)ifs_image + ifs_header->node_table_offset);


  for (t = 0; t < ifs_header->node_cnt; t++) {
    KLog("node: %s inode_nr = %d parent %d", ifs_node->name, ifs_node->inode_nr,
         ifs_node->parent_inode_nr);
    if (StrCmp(ifs_node->name, filename) == 0 &&
        ifs_node->parent_inode_nr == boot_inode_nr) {
      file_found = TRUE;
      break;
    }
    ifs_node++;
  }

  if (file_found == FALSE) {
    KLog("file not found");
    return -1;
  }

  elf_base = (uint8 *)ifs_image + ifs_node->file_offset;
  elf_size = ifs_node->file_size;
  elf_seek(0);


  // Read into bootinfo structure
  
  if (elf_read(ehdr, sizeof(Elf32_EHdr)) != sizeof(Elf32_EHdr)) {
    KLog("failed to read");
    return -1;
  }

  if (!(ehdr->e_ident[EI_MAG0] == ELFMAG0 && ehdr->e_ident[EI_MAG1] == 'E' &&
        ehdr->e_ident[EI_MAG2] == 'L' && ehdr->e_ident[EI_MAG3] == 'F' &&
        ehdr->e_ident[EI_CLASS] == ELFCLASS32 &&
        ehdr->e_ident[EI_DATA] == ELFDATA2LSB && ehdr->e_machine == EM_ARM &&
        ehdr->e_type == ET_EXEC && ehdr->e_phnum > 0)) {
    KLog("not supported ELF");
    return -1;
  }

  phdr_cnt = ehdr->e_phnum;
  phdr_offs = ehdr->e_phoff;
  
  if (phdr_cnt > MAX_PHDR) {
    KLog("Too many phdr");
    return -1;
  }

  elf_seek(phdr_offs);

  KLog("\n\n");
  KLog("Program header cnt: %d", phdr_cnt);
  

  if (elf_read(phdr_table, sizeof(Elf32_PHdr) * phdr_cnt) !=
      sizeof(Elf32_PHdr) * phdr_cnt) {
    KLog("Failed to read");
    return -1;
  }

  for (t = 0; t < phdr_cnt; t++) {
    phdr = &phdr_table[t];

    KLog ("phdr %d", t);

    if (phdr->p_type != PT_LOAD) {    
      KLog("phdr %d != PT_LOAD", t);
      continue;
    }

    if (phdr->p_memsz < phdr->p_filesz) {
      KLog ("memsz < filesz");
      return -1;
    }
    
    segment_base = ALIGN_DOWN(phdr->p_vaddr, 4096) - KERNEL_BASE_VA;
    segment_ceiling =
        ALIGN_UP(phdr->p_vaddr + phdr->p_memsz, 4096) - KERNEL_BASE_VA;

    KLog("segment %d base: %08x, ceil: %08x", t, segment_base, segment_ceiling);
    KLog("memsz: %d, filesz: %d", phdr->p_memsz, phdr->p_filesz);
    
    if (segment_ceiling > *ceiling)
    {
      *ceiling = segment_ceiling;
    }    
    
    memset((void *)segment_base, 0, segment_ceiling - segment_base);

    if (phdr->p_filesz != 0) {
      elf_seek(phdr->p_offset);

      if (elf_read((void *)phdr->p_vaddr, phdr->p_filesz) != phdr->p_filesz) {
        KLog("elf_read failed");
        return -1;
      }
    }
  }

  return 0;
}

/**
 *
 */
static void elf_seek(off_t offset) {
  elf_offset = offset;
}

/**
 *
 */
static ssize_t elf_read(void *buf, size_t sz) {
  memcpy(buf, elf_base + elf_offset, sz);
  return (ssize_t)sz;
}



int StrCmp(const char *s, const char *t) {
  while (*s == *t) {
    /* Should it not return the difference? */

    if (*s == '\0') {
      return *s - *t;
      /* return 0; */
    }

    s++;
    t++;
  }

  return *s - *t;
}


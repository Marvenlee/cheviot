#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/debug.h>
#include <sys/dirent.h>
#include <sys/signal.h>
#include <sys/syscalls.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/syslimits.h>
#include <sys/wait.h>





#define NDEBUG

// Constants
#define ARG_SZ 256

// Globals
extern char **environ;
static char *argv[ARG_SZ];

struct {
    int (*Exec)(char *path, int argc, char **argv, int envc char **env);    

    // Additional functions to load libraries and modules.
    
} loader_api;



// Perhaps this should be in ring 1, can execute arbitrary non-executable files
// Add some basic file permission checks to exec.

int main(int argc, char **argv) {

    // Set up initial process, pass pointer to LD function table (Exec, ELF commands, etc).
    
    // Protect LD ram from closing on Exec.
    
    // Call into initial process
}


int ExecProcess(void)
{
    int fd;
}


int ExecVE(char *path, char **argv, int argc, char **envv) {
  void *entry_point;
  void *stack_pointer;
  void *stack_base;
  int fd;
  

  if (CheckELFHeaders(ei) != 0) {
    close(fd);
    errno = ENOEXEC;
    return -1;
  }  

  if (CopyArgEnv(argv, argc, envv) != 0) {
    Close(ei->fd);
    return -1;
  }
  
  // Close on exec, closes handles, frees memory except loader and argv/envv.
  // Resets signal handlers.
  
  if (CloseOnExec() != 0) {
    close(fd);
    exit(EXIT_FAILURE);
    return -1;
  }
  
  sc = LoadImage(fd, &entry_point);
  
  if ((stack_base = VirtualAlloc((void *)0x30000000, USER_STACK_SZ, PROT_READWRITE | MAP_LAZY)) == NULL) {
    close(fd);
    exit(EXIT_FAILURE);
    return -1;
  }

  stack_pointer = stack_base + USER_STACK_SZ - ALIGN_UP(args.total_size, 16) - 16;

  close(fd);
  
  StartApplication(entry_point, stack_pointer, argv, argc, envv);
  
  return 0;
}





// Move to user-space

int CopyArgEnv(char **argv, int argc, char **envv) {
  char *string_table;
  int remaining;
  char *src;
  char *dst;
  int sz;


  // determine envc

  // determine memory needed for argv, envv strings and tables
    
  buffer = VirtualAlloc(MIN_ADDR, mem_size, PROT_READWRITE | PROT_NOCLOEXEC);
  
  // Copy arrays first
  
  
    
  remaining = &execargs_buf[MAX_ARGS_SZ] - string_table;
  dst = string_table;

  for (int t = 0; t < args->argc; t++) {
    src = argv[t];

    StrLCpy (dst, src, remaining);

    argv[t] = dst;

    sz = StrLen(dst) + 1;
    dst += sz;
    remaining -= sz;
  }
  argv[args->argc] = NULL;

  for (int t = 0; t < args->envc; t++) {
    src = envv[t];

    StrLCpy (dst, src, remaining);

    envv[t] = dst;

    sz = StrLen(dst) + 1;
    dst += sz;
    remaining -= sz;
  }
  envv[args->envc] = NULL;

  args->total_size = dst - execargs_buf;
  args->argv = argv;
  args->envv = envv;

  return 0;

exit:
  return -1;
}






/*
 * CheckELFHeaders();
 *
 * Check that it is a genuine ELF executable.
 */
static int CheckELFHeaders(int fd) {
  Elf32_EHdr ehdr;
  int rc;
  
  rc = read(fd, 0, &ehdr, sizeof(Elf32_EHdr));

  if (rc == sizeof(Elf32_EHdr)) {
    if (ehdr.e_ident[EI_MAG0] == ELFMAG0 && ehdr.e_ident[EI_MAG1] == 'E' &&
        ehdr.e_ident[EI_MAG2] == 'L' && ehdr.e_ident[EI_MAG3] == 'F' &&
        ehdr.e_ident[EI_CLASS] == ELFCLASS32 &&
        ehdr.e_ident[EI_DATA] == ELFDATA2LSB && ehdr.e_type == ET_EXEC &&
        ehdr.e_phnum > 0) {

      return 0;
    }
  }
  
  errno = ENOEXEC;
  return -1;
}

/*
 * LoadProcess();
 */
static int ExecImage (int fd, void **entry_point) {
  int t;
  int rc;
  Elf32_EHdr *ehdr;
  Elf32_PHdr *phdr;
  Elf32_PHdr *phdr_table;
  int32_t phdr_cnt;
  off_t phdr_offs, sec_offs;
  void *sec_addr;
  int32_t sec_file_sz;
  vm_size sec_mem_sz;
  uint32_t sec_prot;
  void *ret_addr;
  void *segment_base;
  void *segment_ceiling;
  Elf32_EHdr ehdr_data;
  Elf32_PHdr phdr_table_data[MAX_PHDR];
  
  phdr_table = phdr_table_data;
  ehdr = &ehdr_data;

  if ((ehdr = (void *)VirtualAlloc (0, sizeof (Elf32_EHdr), PROT_READWRITE)) == NULL)
  	return -1;
  }
  
  rc = ReadFile(ei, TRUE, 0, ehdr, sizeof(Elf32_EHdr));

//  Info ("ELF header read");

  if (rc != sizeof(Elf32_EHdr)) {
    Info ("ELF header could not read all");
    return -1;
  }

  phdr_cnt = ehdr->e_phnum;
  phdr_offs = ehdr->e_phoff;
  *entry_point = (void *)ehdr->e_entry;

  //	if ((phdr_table = (void *)VirtualAlloc (0, sizeof (Elf32_PHdr) *
  // phdr_cnt, PROT_READWRITE)) == NULL)
  //		return -1;

  if (phdr_cnt >= MAX_PHDR) {
    Info ("ELF header to many phdrd");
    return -1;
  }

//  Info ("ELF read phdrs phdr_offs = %d", (int)phdr_offs);

  rc = ReadFile(ei, TRUE, phdr_offs, phdr_table, sizeof(Elf32_PHdr) * phdr_cnt);

  if (rc != sizeof(Elf32_PHdr) * phdr_cnt) {
    return -1;
  }

//  Info ("phdrs read");

  for (t = 0; t < phdr_cnt; t++) {
    phdr = phdr_table + t;

    if (phdr->p_type != PT_LOAD)
      continue;

    sec_addr = (void *)ALIGN_DOWN(phdr->p_vaddr, PAGE_SIZE);
    sec_file_sz = phdr->p_filesz;
    sec_mem_sz = ALIGN_UP(phdr->p_vaddr + phdr->p_memsz, PAGE_SIZE) - (vm_addr)sec_addr;
    sec_offs = phdr->p_offset;
    sec_prot = 0;

    if (sec_mem_sz < sec_file_sz)
      return -1;

    if (phdr->p_flags & PF_X)
      sec_prot |= PROT_EXEC;
    if (phdr->p_flags & PF_R)
      sec_prot |= PROT_READ;
    if (phdr->p_flags & PF_W)
      sec_prot |= PROT_WRITE;

    segment_base = (void *)phdr->p_vaddr;
    segment_ceiling = (void *)phdr->p_vaddr + phdr->p_memsz;
    sec_mem_sz = segment_ceiling - segment_base;

    if (sec_mem_sz != 0) {
      ret_addr = VirtualAlloc(sec_addr, sec_mem_sz, PROT_READWRITE | PROT_EXEC | MAP_FIXED);

      if (ret_addr == NULL)
        return -1;
    }

    if (sec_file_sz != 0) {
      rc = ReadFile(ei, FALSE, sec_offs, (void *)phdr->p_vaddr, sec_file_sz);

      if (rc != sec_file_sz) {
        return -1;
      }
    }

//    VirtualProtect(sec_addr, sec_mem_sz, sec_prot);
  }

  return 0;
}



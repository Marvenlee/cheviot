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

//#define KDEBUG

#include <kernel/arm/elf.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>
#include <string.h>
#include <sys/execargs.h>


#define MAX_PHDR 16

struct ExecInfo {
  bool src_is_image;
  int fd;
  void *image;
};


// FIXME: Need rendez, as will be allocating this before reading ELF.

#define MAX_ARGS_SZ 0x10000
struct Rendez execargs_rendez;
bool execargs_busy = FALSE;
char execargs_buf[MAX_ARGS_SZ];


// Private prototypes

int DoExec(struct ExecInfo *ei, struct execargs *_args);
static int CheckELFHeaders(struct ExecInfo *ei);
static int LoadProcess(struct Process *proc, struct ExecInfo *ei, void **entry_point);
ssize_t ReadFile (struct ExecInfo *ei, off_t offset, void *vaddr, size_t sz);
ssize_t KReadFile (struct ExecInfo *ei, off_t offset, void *vaddr, size_t sz);
int CopyInArgv(struct execargs *_args, struct execargs *args, bool inKernel);
int CopyOutArgv(void *stack_pointer, int stack_size, struct execargs *args);
void ReleaseExecArgs(void);

/*
 * Exec system call.
 */

/*
int Kexec(char *filename) {
  struct ExecInfo ei;
  
  ei.src_is_image = FALSE;
    
  Info("Kexec(%s)", filename);

  if ((ei.fd = Kopen(filename, O_RDONLY, 0)) == -1) {
    Info ("Failed to open file");
    return -ENOENT;
  }

  return DoExec(&ei, NULL);
}
*/




SYSCALL int SysExec(char *filename, struct execargs *_args) {
  int sc;
  struct ExecInfo ei;
    
  Info("Exec(%s. execargs:%08x)", filename, (vm_addr)_args);

  ei.src_is_image = FALSE;
  if ((ei.fd = Open(filename, O_RDONLY, 0)) < 0) {
    Info ("*** Exec failed to open file, fd = %d", ei.fd);
    return -ENOENT;
  }

  /*
    filp = GetFilp();
    vnode = filp->vnode;
    if (IsAllowed(vnode, R_BIT | X_BIT) != 0)
    {
        Close(fd);
        return -EPERM;
    }
*/


  sc = DoExec(&ei, _args);
  
  if (sc != 0) {
    Info ("********* Exec failed to exec, sc = %d", sc);
    Exit(-1);
  }
  
  return sc; 
}

// Remove KExecImage, KExec,
// Remove Exec, replace with fuction to close on exec handles and housekeeping/reseting signal handlers.
// 
// Bootloader loads IFS.exe segments into physmem at boot.
// Similarly for linker-loader
//
// Bootloader passes vaddr/paddr for sections to kernel, inc entry points.
// Kernel starts IFS.exe
// 
// Loader configured to map to top of user-address space.
// Could be in shared library area?
//
// Root is IFS image.
// Root forks to create real IFS process.
// Root waits for /dev/ifs to mount.
// Root execs root.exe
int KExecImage(void *image, struct execargs *_args) {
  void *entry_point;
  void *stack_pointer;
  void *stack_base;
  struct Process *current;
  struct execargs args;
  struct ExecInfo ei;
  
  Info ("KExecImage");
  
  ei.src_is_image = TRUE;
  ei.image = image;
    
  current = GetCurrentProcess();
  
  if (CheckELFHeaders(&ei) != 0) {
    Info ("Check headers failed");
    return -ENOEXEC;
  }

  if (CopyInArgv(&args, _args, TRUE) != 0) {
    Info ("CopyInArgv failed");
    return -ENOMEM;
  }

//  CleanupAddressSpace(&current->as);

  if (LoadProcess(current, &ei, &entry_point) != 0) {
    Info ("KLoadProcess failed");
    ReleaseExecArgs();
    Exit(-1); // no return
    return 0;
  }

  if ((stack_base = VirtualAlloc((void *)0x30000000, USER_STACK_SZ, PROT_READWRITE)) == NULL) {
    // TODO: Free process
    ReleaseExecArgs();
    Info ("Failed to allocate stack");
    
    Exit(-1); // no return
    return 0;
  }

  CopyOutArgv(stack_base, USER_STACK_SZ, &args);

  stack_pointer =
      stack_base + USER_STACK_SZ - ALIGN_UP(args.total_size, 16) - 16;

//  Info("KExecImage Stack = %08x, sp = %08x", (vm_addr)stack_base, (vm_addr)stack_pointer);
  ReleaseExecArgs();
  
//  Info ("KExecImage ReleaseExecArgs done");
//  Info ("---- About to ArchInitExec ----- ");  
  ArchInitExec(current, entry_point, stack_pointer, &args);
  return 0;
}










int DoExec(struct ExecInfo *ei, struct execargs *_args) {
  void *entry_point;
  void *stack_pointer;
  void *stack_base;
  struct Process *current;
  struct execargs args;

  current = GetCurrentProcess();

  Info("DoExec");
  
  

  if (CheckELFHeaders(ei) != 0) {
    Close(ei->fd);
    Info ("CheckELFHeaders failed");
    return -ENOEXEC;
  }

  if (CopyInArgv(&args, _args, FALSE) != 0) {
    Info ("CopyInArgv failed");
    Close(ei->fd);
    return -ENOMEM;
  }

  CleanupAddressSpace(&current->as);

  if (LoadProcess(current, ei, &entry_point) != 0) {
    Info("LoadProcess failed");
    ReleaseExecArgs();
    Close(ei->fd);
    Exit(-1); // no return
    return 0;
  }

  if ((stack_base = VirtualAlloc((void *)0x30000000, USER_STACK_SZ, PROT_READWRITE)) == NULL) {
    // TODO: Free process
    ReleaseExecArgs();
    Close(ei->fd);
    Exit(-1); // no return
    return 0;
  }

  CopyOutArgv(stack_base, USER_STACK_SZ, &args);

  stack_pointer =
      stack_base + USER_STACK_SZ - ALIGN_UP(args.total_size, 16) - 16;

  Close(ei->fd);

  // CloseOnExec (current_process);
  // USigExec (current_process);

  ReleaseExecArgs();  
  ArchInitExec(current, entry_point, stack_pointer, &args);
  return 0;
}




int CopyInArgv(struct execargs *args, struct execargs *_args, bool inkernel) {
  char **argv;
  char **envv;
  char *string_table;
  int remaining;
  char *src;
  char *dst;
  int sz;

//  Info ("CopyInArgv inkernel = %d", inkernel);

  while (execargs_busy == true) {
    TaskSleep(&execargs_rendez);
  }

  execargs_busy = true;

  if (_args == NULL) {
    args->argv = NULL;
    args->argc = 0;
    args->envv = NULL;
    args->envc = 0;
    args->total_size = 0;
    execargs_busy = false;
    TaskWakeupAll(&execargs_rendez);
    return 0;
  }

  if (inkernel == TRUE) {
    MemCpy(args, _args, sizeof *args);
  } else if (CopyIn(args, _args, sizeof *args) != 0) {
    Info ("Copyin args failed, _args=%08x", (vm_addr)_args);
    goto exit;
  }

  // TODO : Ensure less than sizeof MAX_ARGS_SZ

//  Info ("Copyinargs args->argv = %08x, %d", (vm_addr)args->argv, args->argc);
//  Info ("Copyinargs args->envv = %08x, %d", (vm_addr)args->envv, args->envc);

  argv = (char **)execargs_buf;
  envv = (char **)((uint8_t *)argv + (args->argc + 1) * sizeof(char *));


  string_table = (char *)((uint8_t *)envv + (args->envc + 1) * sizeof(char *));

  if (inkernel == TRUE) {
    MemCpy(argv, args->argv, args->argc * sizeof(char *));
    MemCpy(envv, args->envv, args->envc * sizeof(char *));
  } else {
  
      
    if (CopyIn(argv, args->argv, args->argc * sizeof(char *)) != 0) {
      Info ("Copyin failed, argc=%d, argv=%08x", args->argc, (vm_addr)args->argv);
      goto exit;
    }

    if (CopyIn(envv, args->envv, args->envc * sizeof(char *)) != 0) {
      Info ("Copyin failed, envc=%d, envv=%08x", args->envc, (vm_addr)args->envv);
      goto exit;
    }
  }
  
  remaining = &execargs_buf[MAX_ARGS_SZ] - string_table;
  dst = string_table;

  for (int t = 0; t < args->argc; t++) {
    src = argv[t];

    if (inkernel == TRUE) {
      StrLCpy (dst, src, remaining);
    } else if (CopyInString(dst, src, remaining) != 0) {
      Info ("CopyinString argv failed");
      goto exit;
    }

    argv[t] = dst;

    sz = StrLen(dst) + 1;
    dst += sz;
    remaining -= sz;
  }
  argv[args->argc] = NULL;

  for (int t = 0; t < args->envc; t++) {
    src = envv[t];

    if (inkernel == TRUE) {
      StrLCpy (dst, src, remaining);
    } else if (CopyInString(dst, src, remaining) != 0) {
      Info ("CopyinString envv failed");
      goto exit;
    }

    envv[t] = dst;

    sz = StrLen(dst) + 1;
    dst += sz;
    remaining -= sz;
  }
  envv[args->envc] = NULL;

  args->total_size = dst - execargs_buf;
  args->argv = argv;
  args->envv = envv;

//  Info ("Copyinargs end args->argv = %08x, %d", (vm_addr)args->argv, args->argc);
//  Info ("Copyinargs end args->envv = %08x, %d", (vm_addr)args->envv, args->envc);

  return 0;

exit:
  Info ("CopyInArgv failed");
  execargs_busy = false;
  TaskWakeupAll(&execargs_rendez);
  return -EFAULT;
}

int CopyOutArgv(void *stack_base, int stack_size, struct execargs *args) {
  void *args_base;
  vm_size difference;

//  Info("CopyOutArgv");
//  Info(".. args->argv = %08x", (vm_addr)args->argv);
//  Info(".. args->envv = %08x", (vm_addr)args->envv);
  
  args_base = stack_base + stack_size - ALIGN_UP(args->total_size, 16);
  difference = (vm_addr)execargs_buf - (vm_addr) args_base;

  for (int t = 0; t < args->argc; t++) {
    args->argv[t] = (char *)((vm_addr)args->argv[t] - difference);
  }

  for (int t = 0; t < args->envc; t++) {
    args->envv[t] = (char *)((vm_addr)args->envv[t] - difference);
  }

  CopyOut((void *)args_base, execargs_buf, args->total_size);

  args->argv = (char **)((vm_addr)args->argv - difference);
  args->envv = (char **)((vm_addr)args->envv - difference);

  execargs_busy = false;
  TaskWakeupAll(&execargs_rendez);

//  Info("CopyOutArgv DONE");

  return 0;
}

void ReleaseExecArgs(void) {
  execargs_busy = false;
  TaskWakeupAll(&execargs_rendez);
}





/*
 * CheckELFHeaders();
 *
 * Check that it is a genuine ELF executable.
 */
static int CheckELFHeaders(struct ExecInfo *ei) {
  Elf32_EHdr ehdr;
  int rc;

  Info ("CheckELFHeaders");

  rc = KReadFile(ei, 0, &ehdr, sizeof(Elf32_EHdr));

  if (rc == sizeof(Elf32_EHdr)) {
    if (ehdr.e_ident[EI_MAG0] == ELFMAG0 && ehdr.e_ident[EI_MAG1] == 'E' &&
        ehdr.e_ident[EI_MAG2] == 'L' && ehdr.e_ident[EI_MAG3] == 'F' &&
        ehdr.e_ident[EI_CLASS] == ELFCLASS32 &&
        ehdr.e_ident[EI_DATA] == ELFDATA2LSB && ehdr.e_type == ET_EXEC &&
        ehdr.e_phnum > 0) {

//      Info("CheckElfHeaders - Is ELF");
      return 0;
    } else {
      if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != 'E' ||
        ehdr.e_ident[EI_MAG2] != 'L' || ehdr.e_ident[EI_MAG3] != 'F') {

        Info("no ELF magic marker");
      }
        
      if (ehdr.e_ident[EI_CLASS] != ELFCLASS32) {
        Info ("Not ELF32 class");
      }

      if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB) {
        Info ("Not ELF LSB");
      }

      if (ehdr.e_type != ET_EXEC) {
        Info ("Not ELF ET_EXEC");
      }

      if (ehdr.e_phnum == 0) {
        Info ("No ELF program headers");
      }
    }    
  } else {
    Info("CheckElfHeaders - kread failed %d", rc);
  }

  Info("FILE IS NOT EXECUTABLE");
  return -ENOEXEC;
}

/*
 * LoadProcess();
 */


static int LoadProcess(struct Process *proc, struct ExecInfo *ei, void **entry_point) {
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

  Info ("LoadProcess");
  
  phdr_table = phdr_table_data;
  ehdr = &ehdr_data;

  //	if ((ehdr = (void *)VirtualAlloc (0, sizeof (Elf32_EHdr),
  // PROT_READWRITE)) == NULL)
  //		return -1;

  rc = KReadFile(ei, 0, ehdr, sizeof(Elf32_EHdr));

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

  // KRead phdr at a time

  rc = KReadFile(ei, phdr_offs, phdr_table, sizeof(Elf32_PHdr) * phdr_cnt);

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
      rc = ReadFile(ei, sec_offs, (void *)phdr->p_vaddr, sec_file_sz);

      if (rc != sec_file_sz) {
        return -1;
      }
    }

//    VirtualProtect(sec_addr, sec_mem_sz, sec_prot);
  }

  return 0;
}


ssize_t ReadFile (struct ExecInfo *ei, off_t offset, void *vaddr, size_t sz)
{
  size_t nbytes_read;
  struct Process *current;
  
  current = GetCurrentProcess();  

  if (ei->src_is_image == TRUE) {
    MemCpy (vaddr, (uint8_t*)ei->image + offset, sz);
    return sz;    
  }
  
  if (Seek(ei->fd, offset, SEEK_SET) == -1) {
    return -1;
  }

  nbytes_read = Read(ei->fd, vaddr, sz);
  
  return nbytes_read;
}


ssize_t KReadFile (struct ExecInfo *ei, off_t offset, void *vaddr, size_t sz)
{
  size_t nbytes_read;
  struct Process *current;
  
  current = GetCurrentProcess();  
  
  if (Seek(ei->fd, offset, SEEK_SET) == -1) {
    return -1;
  }

// FIXME: Replace kreadfile (no need to read into kernel)  ?????????????
// Or keep KRead


//  nbytes_read = KRead(ei->fd, vaddr, sz);
  
  return nbytes_read;
}








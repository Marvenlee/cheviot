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

#define KDEBUG

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


struct Rendez execargs_rendez;
bool execargs_busy = false;
char execargs_buf[MAX_ARGS_SZ];


// Private prototypes

int do_exec(int fd, struct execargs *_args);
static int check_elf_headers(int fd);
static int load_process(struct Process *proc, int fd, void **entry_point);
ssize_t read_file (int fd, off_t offset, void *vaddr, size_t sz);
ssize_t kread_file (int fd, off_t offset, void *vaddr, size_t sz);



/*
 * Exec system call.
 */
SYSCALL int sys_exec(char *filename, struct execargs *_args)
{
  int sc;
  int fd;
    
  Info("Exec(%s. execargs:%08x)", filename, (vm_addr)_args);

  if ((fd = kopen(filename, O_RDONLY, 0)) < 0) {
    Info ("*** Exec failed to open file, fd = %d", fd);
    return -ENOENT;
  }

  Info ("Opened file\n");
  
  /*
  filp = get_filp();
  vnode = filp->vnode;
  if (is_allowed(vnode, R_BIT | X_BIT) != 0)
  {
      sys_close(fd);
      return -EPERM;
  }
  */

  sc = do_exec(fd, _args);
  
  sys_close(fd);
  
  if (sc == -ENOMEM) {
    Info ("********* Exec failed to exec, sc = %d", sc);
    sys_exit(-1);
  }
  
  return sc; 
}

/*
 *
 */
int do_exec(int fd, struct execargs *_args)
{
  void *entry_point;
  void *stack_pointer;
  void *stack_base;
  struct Process *current;
  struct execargs args;
  int8_t *pool;
  
  current = get_current_process();

  Info("DoExec");

  if (check_elf_headers(fd) != 0) {
    Info ("CheckELFHeaders failed");
    return -ENOEXEC;
  }
  
  if ((pool = alloc_arg_pool()) == NULL)
  {
    Info("AllocArgPool failed\n");
    return -EBUSY;
  }
  
  Info ("CopyInArgv");
  
  if (copy_in_argv(pool, &args, _args) != 0) {
    Info ("CopyInArgv failed");
    free_arg_pool(pool);
    return -EFAULT;
  }

  Info("CleanupAddressSpace");

  CleanupAddressSpace(&current->as);

  Info("LoadProcess");

  if (load_process(current, fd, &entry_point) != 0) {
    Info("LoadProcess failed");
    free_arg_pool(pool);
    return -ENOMEM;
  }


  Info("Allocate Stack");
  
  if ((stack_base = sys_virtualalloc((void *)0x30000000, USER_STACK_SZ, PROT_READWRITE)) == NULL) {
    Info("Allocate stack failed");
    free_arg_pool(pool);
    return -ENOMEM;
  }

  Info("CopyOutArgv");

  copy_out_argv(stack_base, USER_STACK_SZ, &args);
  
  Info("FreeArgPool");
  
  free_arg_pool(pool);
  
  stack_pointer = stack_base + USER_STACK_SZ - ALIGN_UP(args.total_size, 16) - 16;

  // CloseOnExec (current_process);
  // USigExec (current_process);

  Info("ArchInitExec");

  ArchInitExec(current, entry_point, stack_pointer, &args);
  return 0;
}

/*
 *
 */ 
char *alloc_arg_pool(void)
{
  while (execargs_busy == true) {
    TaskSleep(&execargs_rendez);
  }

  execargs_busy = true;
  
  return execargs_buf;    // TODO: support 8 buffers
}

/*
 *
 */
void free_arg_pool(char *pool)
{
  execargs_busy = false;
  TaskWakeupAll(&execargs_rendez);
}

/*
 *
 */
int copy_in_argv(char *pool, struct execargs *args, struct execargs *_args) {
  char **argv;
  char **envv;
  char *string_table;
  int remaining;
  char *src;
  char *dst;
  int sz;

  if (_args == NULL) {
    Info("_args == NULL");
    args->argv = NULL;
    args->argc = 0;
    args->envv = NULL;
    args->envc = 0;
    args->total_size = 0;
    execargs_busy = false;
    TaskWakeupAll(&execargs_rendez);
    return 0;
  }

  if (CopyIn(args, _args, sizeof *args) != 0) {
    Info ("Copyin args failed, _args=%08x", (vm_addr)_args);
    goto cleanup;
  }

  // TODO : Ensure less than sizeof MAX_ARGS_SZ

  argv = (char **)pool;
  envv = (char **)((uint8_t *)argv + (args->argc + 1) * sizeof(char *));
  string_table = (char *)((uint8_t *)envv + (args->envc + 1) * sizeof(char *));

  if (CopyIn(argv, args->argv, args->argc * sizeof(char *)) != 0) {
    Info ("Copyin failed, argc=%d, argv=%08x", args->argc, (vm_addr)args->argv);
    goto cleanup;
  }

  if (CopyIn(envv, args->envv, args->envc * sizeof(char *)) != 0) {
    Info ("Copyin failed, envc=%d, envv=%08x", args->envc, (vm_addr)args->envv);
    goto cleanup;
  }
  
  remaining = &pool[MAX_ARGS_SZ] - string_table;
  dst = string_table;

  for (int t = 0; t < args->argc; t++) {
    src = argv[t];

    if (CopyInString(dst, src, remaining) != 0) {
      Info ("CopyinString argv failed");
      goto cleanup;
    }

    argv[t] = dst;

    sz = StrLen(dst) + 1;
    dst += sz;
    remaining -= sz;
  }

  argv[args->argc] = NULL;

  for (int t = 0; t < args->envc; t++) {
    src = envv[t];

    if (CopyInString(dst, src, remaining) != 0) {
      Info ("CopyinString envv failed");
      goto cleanup;
    }

    envv[t] = dst;

    sz = StrLen(dst) + 1;
    dst += sz;
    remaining -= sz;
  }

  envv[args->envc] = NULL;

  args->total_size = dst - pool;
  args->argv = argv;
  args->envv = envv;
  return 0;

cleanup:
  Info("CopyInArgv failed");
  return -EFAULT;
}

/*
 *
 */
int copy_out_argv(void *stack_base, int stack_size, struct execargs *args) {
  void *args_base;
  vm_size difference;

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

  return 0;
}

/*
 * CheckELFHeaders();
 *
 * Check that it is a genuine ELF executable.
 */
static int check_elf_headers(int fd) {
  Elf32_EHdr ehdr;
  int rc;

  Info ("CheckELFHeaders");

  rc = kread_file(fd, 0, &ehdr, sizeof(Elf32_EHdr));

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
static int load_process(struct Process *proc, int fd, void **entry_point) {
  int t;
  int rc;
  int32_t phdr_cnt;
  off_t phdr_offs, sec_offs;
  void *sec_addr;
  int32_t sec_file_sz;
  vm_size sec_mem_sz;
  uint32_t sec_prot;
  void *ret_addr;
  void *segment_base;
  void *segment_ceiling;

  Elf32_EHdr ehdr;
  Elf32_PHdr phdr;
  
  Info ("LoadProcess");

  rc = kread_file(fd, 0, &ehdr, sizeof(Elf32_EHdr));

  if (rc != sizeof(Elf32_EHdr)) {
    Info ("ELF header could not read");
    return -EIO;
  }

  *entry_point = (void *)ehdr.e_entry;

  phdr_cnt = ehdr.e_phnum;
  phdr_offs = ehdr.e_phoff;
  

  for (t = 0; t < phdr_cnt; t++) {
    
    Info("LoadProcess phdr = %d", t);
    
    rc = kread_file(fd, phdr_offs + t * sizeof(Elf32_PHdr), &phdr, sizeof(Elf32_PHdr));

    if (rc != sizeof(Elf32_PHdr)) {
      Info("Kread phdr failed");
      return -EIO;
    }

    if (phdr.p_type != PT_LOAD) {
      Info("Phdr.p_type !- PT_LOAD");
      continue;
    }
    
    sec_addr = (void *)ALIGN_DOWN(phdr.p_vaddr, PAGE_SIZE);
    sec_file_sz = phdr.p_filesz;
    sec_mem_sz = ALIGN_UP(phdr.p_vaddr + phdr.p_memsz, PAGE_SIZE) - (vm_addr)sec_addr;
    sec_offs = phdr.p_offset;
    sec_prot = 0;

    if (sec_mem_sz < sec_file_sz) {
      Info("sec_mem_sz < file_sz");
      return -EIO;
    }
    
    if (phdr.p_flags & PF_X)
      sec_prot |= PROT_EXEC;
    if (phdr.p_flags & PF_R)
      sec_prot |= PROT_READ;
    if (phdr.p_flags & PF_W)
      sec_prot |= PROT_WRITE;

    segment_base = (void *)phdr.p_vaddr;
    segment_ceiling = (void *)phdr.p_vaddr + phdr.p_memsz;
    sec_mem_sz = segment_ceiling - segment_base;

    if (sec_mem_sz != 0) {
      ret_addr = sys_virtualalloc(sec_addr, sec_mem_sz, PROT_READWRITE | PROT_EXEC | MAP_FIXED);

      if (ret_addr == NULL) {
        Info("Failed to alloc fixed mem");
        return -ENOMEM;
      }
    }

    if (sec_file_sz != 0) {
      rc = read_file(fd, sec_offs, (void *)phdr.p_vaddr, sec_file_sz);

      if (rc != sec_file_sz) {
        Info("Failed to read file");
        return -ENOMEM;
      }
    }

//    sys_virtualprotect(sec_addr, sec_mem_sz, sec_prot);
  }

  return 0;
}



ssize_t read_file (int fd, off_t offset, void *vaddr, size_t sz)
{
  size_t nbytes_read;
  
  if (sys_lseek(fd, offset, SEEK_SET) == -1) {
    return -1;
  }

  nbytes_read = sys_read(fd, vaddr, sz);
  
  return nbytes_read;
}


ssize_t kread_file (int fd, off_t offset, void *vaddr, size_t sz)
{
  size_t nbytes_read;
  
  if (sys_lseek(fd, offset, SEEK_SET) == -1) {
    return -1;
  }

  nbytes_read = kread(fd, vaddr, sz);
  
  return nbytes_read;
}



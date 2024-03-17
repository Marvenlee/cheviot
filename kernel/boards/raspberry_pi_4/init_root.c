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

#include <string.h>
#include <kernel/arch.h>
#include <kernel/board/boot.h>
#include <kernel/board/globals.h>
#include <kernel/board/init.h>
#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>
#include <sys/execargs.h>



// TODO: Move to filesystem/exec_root.c (or exec.c)

/* Forward declarations */
static int LoadRootElf(void *file_base, void **entry_point);
static void *MapIFS(void *vaddr, vm_size sz, void *paddr, bits32_t flags);
static int InitRootArgv(char *pool, struct execargs *args, char *exe_name, void *ifs_base, size_t ifs_size);
ssize_t ReadIFS (void *base, off_t offset, void *vaddr, size_t sz);


/* @Brief Kernel entry point of root process
 *
 * Maps the ELF sections of IFS.exe that were loaded by the bootloader.
 * Maps the IFS file system image.
 * Allocates the initial stack for the process.
 * Starts the root process at the IFS.exe's entry point.
 */
void BootstrapRootProcess(void) {
  struct Process *current;
  void *entry_point;
  void *stack_pointer;
  void *stack_base;
  struct execargs args;
  int8_t *pool;
  void *ifs_base;
  void *ifs_exe_base;
  struct AddressSpace *as;

  Info ("BootstrapRootProcess ...");

#if 1
  Info ("** Enabling interrupts **");
  EnableInterrupts();
#endif  

  current = get_current_process();
  as = &current->as;

  if ((pool = alloc_arg_pool()) == NULL) {
    Info("Root alloc arg pool failed");
    KernelPanic();
  }

  
//  CleanupAddressSpace(&current->as);

/*
  Info ("Map IFS image to 0x00400000");

  if ((ifs_base = MapIFS((void *)0x00400000, bootinfo->ifs_image_size, (void *)bootinfo->ifs_image, PROT_READWRITE)) == NULL) {
    Info("Root map IFS image failed");
    KernelPanic();
  }
*/

/*
  if ((ifs_base = MapIFS((void *)0x00400000, 0x10000, (void *)0x00008000, PROT_READWRITE)) == NULL) {
    Info("Root map IFS image failed");
    KernelPanic();
  }
*/

  Info ("ifs_base phys     = %08x", (vm_addr)bootinfo->ifs_image);  
  Info ("ifs_exe_base phys = %08x", (vm_addr)bootinfo->ifs_exe_base);  
  Info ("ifs_image_size    = %08x", (vm_addr)bootinfo->ifs_image_size);  

  ifs_exe_base = (void *)pmap_pa_to_va((vm_addr)bootinfo->ifs_exe_base);
  
  Info("ifs_exe_base kernel va:%08x", (uint32_t)ifs_exe_base); 
  
  if (LoadRootElf(ifs_exe_base, &entry_point) != 0) {
    Info("LoadProcess failed");
    KernelPanic();
  }

  Info ("entry_point: %08x", (vm_addr)entry_point);

  Info ("allocating root stack");
  
  if ((stack_base = sys_virtualalloc((void *)0x02000000, USER_STACK_SZ, PROT_READWRITE)) == NULL) {
    Info("Root stack alloc failed");
    KernelPanic();
  }

  
  InitRootArgv(pool, &args, "/sbin/ifs", (void *)bootinfo->ifs_image, bootinfo->ifs_image_size);
  
  copy_out_argv(stack_base, USER_STACK_SZ, &args);
  
  free_arg_pool(pool);
   
  stack_pointer = stack_base + USER_STACK_SZ - ALIGN_UP(args.total_size, 32) - 32;
  
  Info ("Stack base   : %08x", stack_base);
  Info ("Stack Pointer: %08x", stack_pointer);
  Info ("Args");
  Info ("Entry Point  : %08x", (vm_addr) entry_point);
  
  arch_init_exec(current, entry_point, stack_pointer, &args);
}


/* @brief Map the IFS.exe sections into the root process.
 *
 * The Bootloader loads the IFS.exe into RAM just below the IFS image.
 * This uses the Elf header and Program header table is passed to the kernel in
 * the BootInfo structure.
 */
int LoadRootElf(void *file_base, void **entry_point)
{
  int rc;
  
  Elf32_EHdr ehdr;
  Elf32_PHdr phdr;
  
  int32_t phdr_cnt;
  off_t phdr_offs, sec_offs;
  void *sec_addr;
  void *sec_paddr;
  int32_t sec_file_sz;         // TODO: Is this not needed, bootloader allocates enough mem size pages?
  vm_size sec_mem_sz;
  uint32_t sec_prot;
  void *ret_addr;
  
  Info ("LoadRootElf file_base=%08x", (uint32_t)file_base);
    
  // Read in ELF header from bootinfo->ifs_exe_base
  
  Info ("..ReadIFS(ehdr) ehdr = %08x", (uint32_t)&ehdr);
  rc = ReadIFS(file_base, 0, &ehdr, sizeof(Elf32_EHdr));

  if (rc != sizeof(Elf32_EHdr)) {
    Info ("ELF header could not read");
    return -EIO;
  }

  // Validate ELF header here

  Info ("CheckELFHeaders");

  if (ehdr.e_ident[EI_MAG0] == ELFMAG0 && ehdr.e_ident[EI_MAG1] == 'E' &&
      ehdr.e_ident[EI_MAG2] == 'L' && ehdr.e_ident[EI_MAG3] == 'F' &&
      ehdr.e_ident[EI_CLASS] == ELFCLASS32 &&
      ehdr.e_ident[EI_DATA] == ELFDATA2LSB && ehdr.e_type == ET_EXEC &&
      ehdr.e_phnum > 0) {

    Info ("root File is ELF");
  } else {
    Info("FILE IS NOT EXECUTABLE");

    Info ("Magic: %02x %02x %02x %02x", 
            ehdr.e_ident[EI_MAG0],
            ehdr.e_ident[EI_MAG1],
            ehdr.e_ident[EI_MAG2],
            ehdr.e_ident[EI_MAG3]);

    KernelPanic();
  }

  Info ("ehdr.e_entry = %08x", ehdr.e_entry);

  *entry_point = (void *)ehdr.e_entry;

  phdr_cnt = ehdr.e_phnum;
  phdr_offs = ehdr.e_phoff;

  for (int t = 0; t < phdr_cnt; t++) {
    
    rc = ReadIFS(file_base, phdr_offs + t * sizeof(Elf32_PHdr), &phdr, sizeof(Elf32_PHdr));

    if (rc != sizeof(Elf32_PHdr)) {
      return -EIO;
    }

    if (phdr.p_type != PT_LOAD) {
      continue;
    }

    sec_addr = (void *)ALIGN_DOWN(phdr.p_vaddr, PAGE_SIZE);
    sec_file_sz = phdr.p_filesz;
    sec_mem_sz = ALIGN_UP(phdr.p_vaddr + phdr.p_memsz, PAGE_SIZE) - (vm_addr)sec_addr;
    sec_offs = phdr.p_offset;
    sec_prot = 0;

    if (sec_mem_sz < sec_file_sz) {
      return -EIO;
    }
    
    if (phdr.p_flags & PF_X)
      sec_prot |= PROT_EXEC;
    if (phdr.p_flags & PF_R)
      sec_prot |= PROT_READ;
    if (phdr.p_flags & PF_W)
      sec_prot |= PROT_WRITE;

//    segment_base = (void *)phdr.p_vaddr;
//    segment_ceiling = (void *)phdr.p_vaddr + phdr.p_memsz;
//    sec_mem_sz = segment_ceiling - segment_base;  // FIXME: why not just use phdr.p_memsz ? ALIGN_UP ??

    Info("root sec_addr    :%08x", (uint32_t) sec_addr);
    Info("root sec_file_sz :%08x", (uint32_t) sec_file_sz);
    Info("root sec_mem_sz  :%08x", (uint32_t) sec_mem_sz);
    Info("--");
    
    if (sec_mem_sz != 0) {
      ret_addr = sys_virtualalloc(sec_addr, sec_mem_sz, PROT_READWRITE | PROT_EXEC | MAP_FIXED);

      if (ret_addr == NULL)
        return -ENOMEM;
    }

    if (sec_file_sz != 0) {
      rc = ReadIFS(file_base, sec_offs, (void *)phdr.p_vaddr, sec_file_sz);

      if (rc != sec_file_sz) {
        return -ENOMEM;
      }
    }

//    sys_virtualprotect(sec_addr, sec_mem_sz, sec_prot);
  }

  return 0;
}



/*
 *
 */
static int InitRootArgv(char *pool, struct execargs *args, char *exe_name, void *ifs_base, size_t ifs_size) {
  char **argv;
  char **envv;
  char *string_table;
  int remaining;
  char *src;
  char *dst;
  int sz;
  char tmp[32];
  
  argv = (char **)pool;
  envv = (char **)((uint8_t *)argv + (3 + 1) * sizeof(char *));
  string_table = (char *)((uint8_t *)envv + (0 + 1) * sizeof(char *));
  
  Info ("argv : %08x", (vm_addr)argv);  
  Info ("envv : %08x", (vm_addr)envv);
  Info ("string_table : %08x", (vm_addr)string_table);

  remaining = &pool[MAX_ARGS_SZ] - string_table;
  dst = string_table;
  
  
  Info ("... argv[0] = %s", exe_name);
  argv[0] = dst;
  StrLCpy(dst, exe_name, remaining);
  sz = StrLen(dst) + 1;
  dst += sz;
  remaining -= sz;
  Info ("... argv[1] = %08x", ifs_base);  
  argv[1] = dst;
  Snprintf(tmp, sizeof tmp, "0x%08x", ifs_base);
  StrLCpy(dst, tmp, remaining);
  sz = StrLen(dst) + 1;
  dst += sz;
  remaining -= sz;

  Info ("... argv[2] = %08x", ifs_size);  
  argv[2] = dst;
  Snprintf(tmp, sizeof tmp, "%u", ifs_size);

	Info ("... tmp:%s", tmp);  
  Info ("dst:%08x, tmp:%08x, remaining:%d", (uint32_t)dst, (uint32_t)tmp, remaining);
  StrLCpy(dst, tmp, remaining);
	char *argv_ifs_sz = dst;
	
  sz = StrLen(dst) + 1;
  dst += sz;
  remaining -= sz;
  

	Info ("argv[2] user ifs_sz: %s", argv_ifs_sz);

  args->argc = 3;
  args->envc = 0;

  argv[args->argc] = NULL; 		// null for last element of argv
  envv[0] = NULL;

  args->total_size = dst - pool;
  args->argv = argv;
  args->envv = envv;
  return 0;

exit:
  Info ("CopyInArgv failed");
  return -EFAULT;
}


/*
 * TODO: For page-sized and aligned copies, use COW 
 */
ssize_t ReadIFS (void *base, off_t offset, void *vaddr, size_t sz)
{
  uint8_t *dst, *src;
  uint8_t tmp;
  
  dst = vaddr;
  src = (uint8_t *)base + offset;
  
  Info ("ReadIFS(base:%08x, offs:%08x)", (uint32_t)base, (uint32_t)offset);
  Info ("        vaddr:%08x, sz:%08x)", (uint32_t)vaddr, (uint32_t)sz);   
  memcpy(vaddr, base+offset, sz);
  
  return sz;
}


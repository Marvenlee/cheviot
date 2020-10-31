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
 * Kernel initialization.
 */

//#define KDEBUG

#include <string.h>
#include <kernel/arch.h>
#include <kernel/arm/boot.h>
#include <kernel/arm/globals.h>
#include <kernel/arm/init.h>
#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>

extern int idle_stack_top;
extern void StartExecProcess(void);


void IdleLoop (void);

/*
 * FIXME: Can we not just start IFS.exe (this then forks (2 IFS.exe) root then exec's
 * init.exe once IFS.exe mounts /
 */
void StartRoot(void) {
  while (root_vnode == NULL) {
    TaskSleep(&root_rendez);
  }

  if (Kexec("/boot/init") != 0) {
    Info ("Root Exec failed");
    KernelPanic();
  }  
}


/*
 *
 */
void StartIFS(void)
{
  char *argv[4];
  char ifs_addr_str[16];
  char ifs_size_str[16];
  void *exe_base;
  struct IFSNode *ifs_inode_table;
  struct IFSNode *node;
  struct IFSHeader *ifs_header;
  struct execargs execargs;

  ifs_header = (struct IFSHeader *)VirtualAllocPhys (NULL, bootinfo->ifs_image_size, PROT_READ | PROT_WRITE, (void *)ifs_image);

  if (ifs_header == NULL) {
    KernelPanic();
  }

  if (ifs_header->magic[0] != 'M' || ifs_header->magic[1] != 'A' ||
      ifs_header->magic[2] != 'G' || ifs_header->magic[3] != 'C') {
    KernelPanic();
  }

  ifs_inode_table =
      (struct IFSNode *)((uint8_t *)ifs_header + ifs_header->node_table_offset);

  node = NULL;
  for (int inode_nr = 0; inode_nr < ifs_header->node_cnt; inode_nr++) {
    if ((ifs_inode_table[inode_nr].permissions & _IFREG) && StrCmp(ifs_inode_table[inode_nr].name, "ifs") == 0) {
      node = &ifs_inode_table[inode_nr];        
      break;
    }
  }
  
  if (node == NULL) {
    Error("ifs not found");
    KernelPanic();
  }

  Snprintf (ifs_addr_str, sizeof ifs_addr_str, "0x%08x", (vm_addr)ifs_header);  
  Snprintf (ifs_size_str, sizeof ifs_size_str, "0x%08x", bootinfo->ifs_image_size);  

  argv[0] = "/boot/ifs";
  argv[1] = ifs_addr_str;
  argv[2] = ifs_size_str;
  argv[3] = NULL;

  execargs.argv = argv;
  execargs.argc = 3;
  execargs.envv = NULL;
  execargs.envc = 0;
  
  exe_base = (uint8_t *)ifs_header + node->file_offset; 
  
  Info ("Execing IFS image...");
  KExecImage(exe_base, &execargs);
  
  Info ("... execing failed!");
  while (1) {
    ;
  }
}
     
/*
 *
 */ 
void StartIdle(void)
{
//  DisableInterrupts();
  KernelUnlock();
  EnableInterrupts();
    
  while(1);
}



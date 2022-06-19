#define KDEBUG
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
 * Architecture-neutral code to create and close InterruptHandler objects
 * Used by device drivers to receive notification of interrupts.
 */

#include <kernel/arm/globals.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>

/*
 * Creates and adds an interrupt handler notification object for the
 * specified IRQ. Interrupt handler returns the event handle that will
 * be raised when an interrupt occurs.
 */

SYSCALL int sys_createinterrupt(int irq, void (*callback)(int irq, struct InterruptAPI *api)) {
  struct Process *current;
  struct Filp *filp;
  struct SuperBlock *sb;
  struct VNode *vnode;
  int fd;
  int error;
  
  current = GetCurrentProcess();

/*
  if (!(current->flags & PROCF_ALLOW_IO)) {
    Info ("irq EPERM");
    return -EPERM;
  }
*/

  if (irq < 0) {
    return -EINVAL;
  }
  
  sb = AllocSuperBlock();

  if (sb == NULL) {
    error = -ENOMEM;
    goto exit;
  }

  // Server vnode set to -1.
  vnode = VNodeNew(sb, -1);

  if (vnode == NULL) {
    error = -ENOMEM;
    goto exit;
  }
  
  fd = AllocHandle();
  
  if (fd < 0) {
    error = -ENOMEM;
    goto exit;
  }
  
  filp = GetFilp(fd);

  vnode->flags = V_VALID;
  vnode->reference_cnt = 1;
  vnode->mode = 0777 | _IFIRQ;    // FIXME:  read/write only user  (not used?)
  vnode->isr_irq = irq;
  vnode->isr_callback = callback;
  vnode->isr_process = current;
  filp->vnode = vnode;
  
  DisableInterrupts();
  
  LIST_ADD_TAIL(&isr_handler_list[irq], vnode,
                isr_handler_entry);

  // Convert to list empty test
//  if (irq_handler_cnt[irq] == 0) {
    sys_unmaskinterrupt(irq);
//  }

  irq_handler_cnt[irq]++;
  EnableInterrupts();  
  return fd;
  
exit:
  FreeHandle(fd);
  VNodeFree(vnode);
  FreeSuperBlock(sb);  
  return error;
}

/*
 * Called by CloseHandle() to free an interrupt handler.  If there are no
 * handlers for a given IRQ the interrupt is masked.
 *
 * TODO: Need to prevent forking of interrupt descriptors (dup in same process is ok)
 */

int DoCloseInterrupt(int fd) {
  struct Process *current;
  struct VNode *vnode;
  struct SuperBlock *sb;
  struct Filp *filp;
  int irq;
  
  current = GetCurrentProcess();

/*
  if (!(current->flags & PROCF_ALLOW_IO)) {
    return -EPERM;
  }
*/

  filp = GetFilp(fd);
  
  if (filp == NULL) {
    return -EINVAL;
  }

  vnode = filp->vnode;

  if (S_ISIRQ(vnode->mode) == false) {
    return -EINVAL;
  }

  irq = vnode->isr_irq;

  DisableInterrupts();

  LIST_REM_ENTRY(&isr_handler_list[irq], vnode, isr_handler_entry);
  irq_handler_cnt[irq]--;

  if (irq_handler_cnt[irq] == 0)
  {
    sys_maskinterrupt(irq);
  }

  EnableInterrupts();

  FreeHandle(fd);
  VNodeFree(vnode);
  FreeSuperBlock(sb);

  return 0;
}


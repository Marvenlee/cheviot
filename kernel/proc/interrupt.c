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

#include <kernel/board/globals.h>
#include <kernel/arch.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>


/*
 * Creates and adds an interrupt handler notification object for the
 * specified IRQ. Interrupt handler returns the event handle that will
 * be raised when an interrupt occurs.
 *
 * Need to check if process is part of device_driver group
 */
int sys_createinterrupt(int irq, void (*callback)(int irq, struct InterruptAPI *api))
{
  struct Process *current;
  struct ISRHandler *isrhandler;
  int fd;
  
  current = get_current_process();

  if (irq < 0 || irq > NIRQ) {
    return -EINVAL;
  }
  
  fd = alloc_fd_isrhandler(current);
  
  if (fd < 0) {
    return -ENOMEM;
  }
  
  isrhandler = get_isrhandler(current, fd);

  isrhandler->isr_irq = irq;
  isrhandler->isr_callback = callback;
  isrhandler->isr_process = current;
  isrhandler->pending_dpc = false;
  
  DisableInterrupts();
  
  LIST_ADD_TAIL(&isr_handler_list[irq], isrhandler,
                isr_handler_entry);

  // Convert to list empty test
//  if (irq_handler_cnt[irq] == 0) {
    sys_unmaskinterrupt(irq);
//  }

  irq_handler_cnt[irq]++;
  EnableInterrupts();  
  return fd;
}

/*
 * Called by CloseHandle() to free an interrupt handler.  If there are no
 * handlers for a given IRQ the interrupt is masked.
 *
 * TODO: Need to prevent forking of interrupt descriptors (dup in same process is ok)
 */
int close_isrhandler(struct Process *proc, int fd)
{
  struct ISRHandler *isrhandler;
  int irq;
  
  isrhandler = get_isrhandler(proc, fd);
  
  if (isrhandler == NULL) {
    return -EINVAL;
  }

  irq = isrhandler->isr_irq;

  DisableInterrupts();

  LIST_REM_ENTRY(&isr_handler_list[irq], isrhandler, isr_handler_entry);
  irq_handler_cnt[irq]--;

  if (irq_handler_cnt[irq] == 0)
  {
    sys_maskinterrupt(irq);
  }

  EnableInterrupts();

  free_fd_isrhandler(proc, fd);
  return 0;
}


/*
 *
 */
struct ISRHandler *get_isrhandler(struct Process *proc, int fd)
{
  struct Filp *filp;
  
  filp = get_filp(proc, fd);
  
  if (filp == NULL) {
    return NULL;
  }
  
  if (filp->type != FILP_TYPE_ISRHANDLER) {
    return NULL;
  }
  
  return filp->u.isrhandler;
}


/*
 * Allocates a handle structure.
 */
int alloc_fd_isrhandler(struct Process *proc)
{
  int fd;
  struct ISRHandler *isrhandler;
  
  fd = alloc_fd_filp(proc);
  
  if (fd < 0) {
    return -EMFILE;
  }
  
  isrhandler = alloc_isrhandler();
  
  if (isrhandler == NULL) {
    free_fd_filp(proc, fd);
    return -EMFILE;
  }
  
  set_fd(proc, fd, FILP_TYPE_ISRHANDLER, FD_FLAG_CLOEXEC, isrhandler);  
  return fd;
}


/*
 * Returns a handle to the free handle list.
 */
int free_fd_isrhandler(struct Process *proc, int fd)
{
  struct ISRHandler *isrhandler;
  
  isrhandler = get_isrhandler(proc, fd);

  if (isrhandler == NULL) {
    return -EINVAL;
  }

  free_isrhandler(isrhandler);
  free_fd_filp(proc, fd);
  return 0;
}


/*
 *
 */
struct ISRHandler *alloc_isrhandler(void)
{
  struct ISRHandler *isrhandler;

  isrhandler = LIST_HEAD(&isr_handler_free_list);

  if (isrhandler == NULL) {
    return NULL;
  }

  LIST_REM_HEAD(&isr_handler_free_list, free_link);
  isrhandler->reference_cnt = 1;
  return isrhandler;
}


/*
 *
 */
void free_isrhandler(struct ISRHandler *isrhandler)
{
  if (isrhandler == NULL) {
    return;
  }

  isrhandler->reference_cnt--;
  
  if (isrhandler->reference_cnt == 0) {
    LIST_ADD_HEAD(&isr_handler_free_list, isrhandler, free_link);
  }
}



/* @brief   Put a knote on a kqueue's pending list from an interrupt handler
 *
 * struct InterruptAPI should have a context pointer, to point to allocated interrupt struct
 * This will have the knote list
 *
 * Going to need spinlocks or interrupts disabled around parts of adding entry to pending queue
 * and checking if item already pending 
 *
 * Also removal (and perhaps disable) of knotes (and underlying interrupt_handler) will need
 * to be careful that an interrupt isn't already in-flight.
 *
 * FIXME: Do we defer this onto high-priority DPC task, the same as timers?
 *
 * Move into interrupt.c, rename isr_postprocessing()
 * Add isr_dpc_task in interrupt.c, call knote_isr_dpc()
 */
int knote_from_isr(struct InterruptAPI *api, int hint)
{
  struct ISRHandler *isr_handler = api->context;
  
  if (isr_handler->pending_dpc == false) {
    LIST_ADD_TAIL(&pending_isr_dpc_list, isr_handler, pending_isr_dpc_link);
    isr_handler->pending_dpc = true;
  }
  
  TaskWakeupFromISR(&interrupt_dpc_rendez);
  return 0;
}


/* @brief  Defer processing of interrupt kqueue events onto high priority task.
 *
 */
void interrupt_dpc(void)
{
  struct ISRHandler *isr_handler;
  int_state_t int_state;

  while(1) {
    int_state = DisableInterrupts();
   
    while((isr_handler = LIST_HEAD(&pending_isr_dpc_list)) == NULL) {    
      TaskSleep(&interrupt_dpc_rendez);
    }
    
    LIST_REM_HEAD(&pending_isr_dpc_list, pending_isr_dpc_link);
    isr_handler->pending_dpc = false;
    RestoreInterrupts(int_state);
    
    knote(&isr_handler->knote_list, NOTE_MSG);
  }
}


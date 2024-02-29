
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
 * Raspberry Pi interrupt handling functions.
 */

#include <kernel/board/arm.h>
#include <kernel/board/interrupt.h>
#include <kernel/board/timer.h>
#include <kernel/board/globals.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/timer.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/interrupt.h>


/*
 *
 */
void interrupt_handler(struct UserContext *context)
{
  bool unlock = false;
  
  if (bkl_locked == false) {
    KernelLock();
    unlock = true;
  }

  interrupt_top_half();

  if (unlock == true) {
    KernelUnlock();
    // FIXME: Check for pending signals before returning.
    // CheckSignals(context);
  }
}


/*
 *
 */
void interrupt_top_half(void)
{
  int irq;
  struct ISRHandler *isr_handler;
  struct Process *current;
  
  current = get_current_process();

  save_pending_interrupts();
  
  if (check_pending_interrupt(INTERRUPT_TIMER3)) {
    interrupt_top_half_timer();   
    clear_pending_interrupt(INTERRUPT_TIMER3);
  }
  
  irq = 0;

  while (irq < NIRQ) {
    if (get_pending_interrupt_word(irq) == 0) {      
      irq += 32;
      continue;
    }

    if (check_pending_interrupt(irq) == true) {
      isr_handler = LIST_HEAD(&isr_handler_list[irq]);

      while (isr_handler != NULL) {        
        if (isr_handler->isr_callback != NULL) {            
          if (isr_handler->isr_callback == NULL) {
            interruptapi_mask_interrupt(irq);
            interruptapi_knotei(&interrupt_api, NOTE_INT);  
          } else {
            pmap_switch(isr_handler->isr_process, current);                
            interrupt_api.context = isr_handler;
            isr_handler->isr_callback(irq, &interrupt_api);
            pmap_switch(current, isr_handler->isr_process);
          }
        }

        isr_handler = LIST_NEXT(isr_handler, isr_handler_entry);
      }

      clear_pending_interrupt(irq);
    }

    irq++;
  }
}


/*
 *
 */
void save_pending_interrupts(void)
{
  pending_interrupts[0] |= interrupt_regs->irq_pending_1;
  pending_interrupts[1] |= interrupt_regs->irq_pending_2;
  pending_interrupts[2] |= interrupt_regs->irq_basic_pending;
}


/*
 *
 */
bool check_pending_interrupt(int irq)
{
  return ((pending_interrupts[irq / 32] & (1 << (irq % 32))) != 0) ? true : false;  
}


/*
 *
 */
void clear_pending_interrupt(int irq)
{
   pending_interrupts[irq / 32] &= ~(1 << (irq % 32));

  // TODO: Where is the EOI ?
  // TODO: Does ARMC interrupt controller need an EOI command? 
}


/*
 *
 */
uint32_t get_pending_interrupt_word(int irq)
{
  return pending_interrupts[irq / 32];
}


/*
 * Unmasks an IRQ line
 */
void enable_irq(int irq)
{
  if (irq < 32) {
    hal_mmio_write(&interrupt_regs->enable_irqs_1, 1 << irq);
  } else if (irq < 64) {
    hal_mmio_write(&interrupt_regs->enable_irqs_2, 1 << (irq - 32));
  } else {
    hal_mmio_write(&interrupt_regs->enable_basic_irqs, 1 << (irq - 64));
  }
}


/*
 * Masks an IRQ line
 */
void disable_irq(int irq)
{
  if (irq < 32) {
    hal_mmio_write(&interrupt_regs->disable_irqs_1, 1 << irq);
  } else if (irq < 64) {
    hal_mmio_write(&interrupt_regs->disable_irqs_2, 1 << (irq - 32));
  } else {
    hal_mmio_write(&interrupt_regs->disable_basic_irqs, 1 << (irq - 64));
  }
}



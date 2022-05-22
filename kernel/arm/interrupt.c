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

#include <kernel/arm/arm.h>
#include <kernel/arm/globals.h>
#include <kernel/arm/raspberry.h>
#include <kernel/arm/raspberry.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/timer.h>
#include <kernel/types.h>
#include <kernel/utility.h>

// FIXME: Move all into raspberry.c
void dmb(void);
void TimerInterruptHandler(void);
void EnableIRQ(int irq);
void DisableIRQ(int irq);
int IsInterruptPending(int irq);
void ProcessInterrupts(int imin, int imax);

volatile int dispatch_level = 0;

extern struct InterruptAPI interrupt_api;



/*
 *
 */
void InterruptHandler(struct UserContext *context) {
  bool unlock = false;
  
  if (bkl_locked == false) {
    KernelLock();
    unlock = true;
  }

  InterruptTopHalf();

  if (unlock == true) {
    KernelUnlock();
//  CheckSignals(context);
  }
}


/*
 *
 */
void InterruptTopHalf(void) {
  int irq;
  struct VNode *isr_handler;
  struct Process *current;
  
  current = GetCurrentProcess();

  dmb();  
  pending_interrupts[0] |= interrupt_regs->irq_pending_1;
  pending_interrupts[1] |= interrupt_regs->irq_pending_2;
  pending_interrupts[2] |= interrupt_regs->irq_basic_pending;
  dmb();

  if (pending_interrupts[0] & (1 << INTERRUPT_TIMER3)) {
    TimerInterruptHandler();

    pending_interrupts[0] &= ~(1 << INTERRUPT_TIMER3);
  }
  
  irq = 0;

  while (irq < NIRQ) {
    if (pending_interrupts[irq / 32] == 0) {
      irq += 32;
      continue;
    }

    if (pending_interrupts[irq / 32] & (1 << (irq % 32))) {
      isr_handler = LIST_HEAD(&isr_handler_list[irq]);

      if (isr_handler != NULL) {
        while (isr_handler != NULL) {
        
          if (isr_handler->isr_callback != NULL) {            
            if (isr_handler->isr_callback == NULL) {
              MaskInterruptFromISR(irq);
              PollNotifyFromISR(&interrupt_api, POLLPRI, POLLPRI);  
              dmb();
            } else {
              PmapSwitch(isr_handler->isr_process, current);                
              interrupt_api.interrupt_vnode = isr_handler;
              dmb();
              isr_handler->isr_callback(irq, &interrupt_api);
              dmb();              
              PmapSwitch(current, isr_handler->isr_process);
            }
          }

          isr_handler = LIST_NEXT(isr_handler, isr_handler_entry);
        }
        
      }
      
      pending_interrupts[irq / 32] &= ~(1 << (irq % 32));
      
    }

    irq++;
  }
}


/*
 *
 */
void TimerInterruptHandler(void) {
  uint32_t clo;
  uint32_t status;

  dmb();
  status = timer_regs->cs;
  dmb();

  if (status & ST_CS_M3) {
    clo = timer_regs->clo;
    clo += MICROSECONDS_PER_JIFFY;
    timer_regs->c3 = clo;
  dmb();

    timer_regs->cs = ST_CS_M3;
  dmb();

    TimerTopHalf();
  }
}


/*
 * MaskInterrupt();
 *
 * Masks an IRQ line.
 * Only MaskInterrupt() and UnmaskInterrupt() system calls should be called from
 * within interrupt handlers.
 */

SYSCALL int SysMaskInterrupt(int irq) {
//  struct Process *current;

//  current = GetCurrentProcess();

/*
  if (!(current->flags & PROCF_ALLOW_IO)) {
    return -EPERM;
  }
*/

  if (irq < 0 || irq >= NIRQ) {
    return -EINVAL;
  }

  DisableInterrupts();

  if (irq_mask_cnt[irq] < 0x80000000) {
    irq_mask_cnt[irq]++;
  }
  
  if (irq_mask_cnt[irq] > 0) {
    DisableIRQ(irq);
  }

  EnableInterrupts();

  return irq_mask_cnt[irq];
}


/*
 * UnmaskInterrupt();
 *
 * Unmasks an IRQ line.
 * Only MaskInterrupt() and UnmaskInterrupt() system calls should be called from
 * within interrupt handlers.
 */

SYSCALL int SysUnmaskInterrupt(int irq) {
//  struct Process *current;

//  current = GetCurrentProcess();

/*  if (!(current->flags & PROCF_ALLOW_IO)) {
    return -EPERM;
  }
*/

  if (irq < 0 || irq >= NIRQ) {
    return -EINVAL;
  }

  DisableInterrupts();

  if (irq_mask_cnt[irq] > 0) {
    irq_mask_cnt[irq]--;
  }
  
  if (irq_mask_cnt[irq] == 0) {
    EnableIRQ(irq);
  }

  EnableInterrupts();

  return irq_mask_cnt[irq];
}



/*
 * MaskInterrupt();
 *
 * Masks an IRQ line.
 * Only MaskInterrupt() and UnmaskInterrupt() system calls should be called from
 * within interrupt handlers.
 */

int MaskInterruptFromISR(int irq) {
  if (irq < 0 || irq >= NIRQ) {
    return -EINVAL;
  }

  if (irq_mask_cnt[irq] < 0x80000000) {
    irq_mask_cnt[irq]++;
  }
  
  if (irq_mask_cnt[irq] > 0) {
    DisableIRQ(irq);
  }

  return irq_mask_cnt[irq];
}


/*
 * UnmaskInterrupt();
 *
 * Unmasks an IRQ line.
 * Only MaskInterrupt() and UnmaskInterrupt() system calls should be called from
 * within interrupt handlers.
 */

int UnmaskInterruptFromISR(int irq) {
  if (irq < 0 || irq >= NIRQ) {
    return -EINVAL;
  }

  if (irq_mask_cnt[irq] > 0) {
    irq_mask_cnt[irq]--;
  }
  
  if (irq_mask_cnt[irq] == 0) {
    EnableIRQ(irq);
  }
  
  return irq_mask_cnt[irq];
}




/*
 * Unmasks an IRQ line
 */

void EnableIRQ(int irq) {
  dmb();
  if (irq < 32) {
    interrupt_regs->enable_irqs_1 = 1 << irq;
  } else if (irq < 64) {
    interrupt_regs->enable_irqs_2 = 1 << (irq - 32);
  } else {
    interrupt_regs->enable_basic_irqs = 1 << (irq - 64);
  }
  dmb();
}

/*
 * Masks an IRQ line
 */

void DisableIRQ(int irq) {
  dmb();
  if (irq < 32) {
    interrupt_regs->disable_irqs_1 = 1 << irq;
  } else if (irq < 64) {
    interrupt_regs->disable_irqs_2 = 1 << (irq - 32);
  } else {
    interrupt_regs->disable_basic_irqs = 1 << (irq - 64);
  }
  dmb();
}




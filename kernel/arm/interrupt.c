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
 
#include <kernel/types.h>
#include <kernel/proc.h>
#include <kernel/error.h>
#include <kernel/dbg.h>
#include <kernel/timer.h>
#include <kernel/arm/arm.h>
#include <kernel/arm/raspberry.h>
#include <kernel/globals.h>
#include <kernel/utility.h>



// Move all into raspberry.c

void EnableIRQ (int irq);
void DisableIRQ (int irq);
int IsInterruptPending (int irq);
void ProcessInterrupts (int imin, int imax);





/*
 * 
 */

void InterruptTopHalf (void)
{
    uint32 clo;
    uint32 status;
    struct CPU *cpu;
    
    MemoryBarrier();
    
    pending_interrupts[0] |= interrupt_regs->irq_pending_1;
    pending_interrupts[1] |= interrupt_regs->irq_pending_2;
    pending_interrupts[2] |= interrupt_regs->irq_basic_pending;

    if (pending_interrupts[0] & (1 << INTERRUPT_TIMER3))
    {
        pending_interrupts[0] &= ~(1 << INTERRUPT_TIMER3);
        MemoryBarrier();
        status = timer_regs->cs;
        MemoryBarrier();
        
        if (status & ST_CS_M3)
        {
            clo = timer_regs->clo;
            clo += MICROSECONDS_PER_JIFFY;
            timer_regs->c3 = clo;            
            MemoryBarrier();
            
            timer_regs->cs = ST_CS_M3;
            MemoryBarrier();
        
            TimerTopHalf();
        }
    }
    
    MemoryBarrier();

    mask_interrupts[0] |= pending_interrupts[0];
    mask_interrupts[1] |= pending_interrupts[1];
    mask_interrupts[2] |= pending_interrupts[2];

    interrupt_regs->disable_irqs_1 = mask_interrupts[0];
    interrupt_regs->disable_irqs_2 = mask_interrupts[1];
    interrupt_regs->disable_basic_irqs = mask_interrupts[2];
    MemoryBarrier();
            
//    KLog ("-- intr TOP END -- pending = %08x, masked = %08x", interrupt_regs->irq_pending_1,interrupt_regs->disable_irqs_1);
    cpu = GetCPU();
    cpu->reschedule_request = TRUE;
}



        
/*
 *
 */

void InterruptBottomHalf (void)
{
    DisableInterrupts();

//    KLog ("-- Bottom -- pending = %08x, masked = %08x", interrupt_regs->irq_pending_1,interrupt_regs->disable_irqs_1);
    
    mask_interrupts[0] &= ~pending_interrupts[0];
    mask_interrupts[1] &= ~pending_interrupts[1];
    mask_interrupts[2] &= ~pending_interrupts[2];
    
    interrupt_regs->disable_irqs_1 = mask_interrupts[0];
    interrupt_regs->disable_irqs_2 = mask_interrupts[1];
    interrupt_regs->disable_basic_irqs = mask_interrupts[2];
    MemoryBarrier();

    if (pending_interrupts[0] != 0)
        ProcessInterrupts(0, 31);
   
    if (pending_interrupts[1] != 0)
        ProcessInterrupts(32, 63);
    
    if (pending_interrupts[2] != 0)
        ProcessInterrupts(64, NIRQ-1);

//    KLog ("-- Bottom END -- pending = %08x, masked = %08x", interrupt_regs->irq_pending_1,interrupt_regs->disable_irqs_1);

    EnableInterrupts();
}




/*
 *
 */

void ProcessInterrupts (int imin, int imax)
{
    int t;
    struct ISRHandler *isr_handler;

//    KLog ("ProcessInterrupts, min = %d, max = %d", imin, imax);
       
    for (t = imin; t <= imax; t++)
    {
        if (IsInterruptPending(t) == TRUE)
        {
            isr_handler = LIST_HEAD(&isr_handler_list[t]);
            
            while (isr_handler != NULL)
            {
                if (irq_mask_cnt[t] < 0x80000000)
                    irq_mask_cnt[t] ++;

                if (irq_mask_cnt[t] > 0)
                    DisableIRQ(t);
    
                DoRaiseEvent (isr_handler->handle);
                isr_handler = LIST_NEXT(isr_handler, isr_handler_entry);
            }
        }
    }
}




/*
 *
 */
 
int IsInterruptPending (int irq)
{
    bits32_t pending;

    if (irq >=0 && irq <= 31)
    {
        pending = pending_interrupts[0];

        if (pending & (1<<irq))
            return TRUE;
    }
    else if (irq >= 32 && irq <= 63)
    {
        irq -= 32;
        pending = pending_interrupts[1];
        if (pending & (1<<irq))
            return TRUE;
        
    }
    else if (irq >= 64 && irq <= 72)
    {
        irq -= 64;
        pending = pending_interrupts[2];
        if (pending & (1<<irq))
            return TRUE;

    }

    return FALSE;
}



    



/*
 * MaskInterrupt();
 *
 * Masks an IRQ line.
 * Only MaskInterrupt() and UnmaskInterrupt() system calls should be called from
 * within interrupt handlers.
 */

SYSCALL int MaskInterrupt (int irq)
{
    struct Process *current;

  
    current = GetCurrentProcess();
    
    if (!(current->flags & PROCF_ALLOW_IO))
        return privilegeErr;

    if (irq < 0 || irq >= NIRQ)
        return paramErr;
    
    DisableInterrupts();
    
    if (irq_mask_cnt[irq] < 0x80000000)
        irq_mask_cnt[irq] ++;

    if (irq_mask_cnt[irq] > 0)
        DisableIRQ(irq);
    
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

SYSCALL int UnmaskInterrupt (int irq)
{
    struct Process *current;

    
    current = GetCurrentProcess();
    
    if (!(current->flags & PROCF_ALLOW_IO))
        return privilegeErr;
    
    if (irq < 0 || irq >= NIRQ)
        return paramErr;
        
    DisableInterrupts();
    
    if (irq_mask_cnt[irq] > 0)
        irq_mask_cnt[irq] --;

    if (irq_mask_cnt[irq] == 0)
        EnableIRQ (irq);

    EnableInterrupts();
    
    return irq_mask_cnt[irq];
}




/*
 * Unmasks an IRQ line
 */
 
void EnableIRQ (int irq)
{
    KLog ("EnableIRQ %d", irq);
    if (irq < 32)
    {
        interrupt_regs->enable_irqs_1 = 1 << irq;
    }
    else if (irq < 64)
    {
        interrupt_regs->enable_irqs_2 = 1 << (irq - 32);
    }
    else
    {
        interrupt_regs->enable_basic_irqs = 1 << (irq - 64);
    }
}




/*
 * Masks an IRQ line
 */
 
void DisableIRQ (int irq)
{
    if (irq < 32)
    {
        interrupt_regs->disable_irqs_1 = 1 << irq;
    }
    else if (irq < 64)
    {
        interrupt_regs->disable_irqs_2 = 1 << (irq - 32);
    }
    else
    {
        interrupt_regs->disable_basic_irqs = 1 << (irq - 64);
    }
}






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
 *
 * handle = AddInterruptHandle(IRQ_OF_DEVICE);
 *
 * while (1)
 * {
 *     WaitEvent (handle);
 *
 *     // Interrupt is masked when it occurs and must be unmasked by process.
 *     // Handle interrupt before unmasking interrupt
 *
 *     UnmaskInterrupt (IRQ_OF_DEVICE);
 *
 *     // Other processing if needed
 * }
 *
 */
 
#include <kernel/types.h>
#include <kernel/proc.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>



/*
 * Creates and adds an interrupt handler notification object for the
 * specified IRQ. Interrupt handler returns the event handle that will
 * be raised when an interrupt occurs.
 */
 
SYSCALL int CreateInterrupt (int irq)
{
    struct ISRHandler *isr_handler;
    struct Process *current;
    int handle;
    
    current = GetCurrentProcess();
    
    if (!(current->flags & PROCF_ALLOW_IO))
        return privilegeErr;

    if (free_handle_cnt < 1 || free_isr_handler_cnt < 1)
        return resourceErr;
        
    handle = AllocHandle();

    isr_handler = LIST_HEAD (&free_isr_handler_list);
    LIST_REM_HEAD (&free_isr_handler_list, isr_handler_entry);
    free_isr_handler_cnt --;
    
    isr_handler->irq = irq;
    isr_handler->handle = handle;
                
    SetObject (current, handle, HANDLE_TYPE_ISR, isr_handler);
    
    DisableInterrupts();
                
    LIST_ADD_TAIL (&isr_handler_list[isr_handler->irq], isr_handler, isr_handler_entry);
    irq_handler_cnt[irq] ++;
    
    if (irq_handler_cnt[irq] == 1)
        UnmaskInterrupt(irq);
    
    EnableInterrupts();
                
    return handle;
}

 


/*
 * Called by CloseHandle() to free an interrupt handler.  If there are no
 * handlers for a given IRQ the interrupt is masked.
 */

int DoCloseInterruptHandler (int handle)
{
    struct ISRHandler *isr_handler;
    struct Process *current;
    int irq;
    
    current = GetCurrentProcess();

    if ((isr_handler = GetObject(current, handle, HANDLE_TYPE_ISR)) == NULL)
        return paramErr;

    DisablePreemption();    
    
    irq = isr_handler->irq;

    DisableInterrupts();

    LIST_REM_ENTRY (&isr_handler_list[irq], isr_handler, isr_handler_entry);
    irq_handler_cnt[irq] --;
    
    if (irq_handler_cnt[irq] == 0)
        MaskInterrupt(irq);
    
    EnableInterrupts();
        
    LIST_ADD_HEAD (&free_isr_handler_list, isr_handler, isr_handler_entry);
    free_isr_handler_cnt ++;
    
    FreeHandle (handle);

    return 0;
}







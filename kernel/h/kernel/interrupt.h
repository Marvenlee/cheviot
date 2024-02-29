#ifndef KERNEL_INTERRUPT_H
#define KERNEL_INTERRUPT_H


#include <kernel/lists.h>
#include <kernel/types.h>
#include <kernel/kqueue.h>


// Forward declarations
struct Process;
struct ISRHandler;

// List types
LIST_TYPE(ISRHandler, isr_handler_list_t, isr_handler_link_t);

/*
 * @brief   Interrupt handler callback
 */
struct ISRHandler
{
  isr_handler_link_t free_link;
  isr_handler_link_t isr_handler_entry;   // Entry within a shared IRQ

  int isr_irq;
  int priority;
  int flags;
  int reference_cnt;
  struct Process *isr_process;
  void (*isr_callback)(int irq, struct InterruptAPI *api);

  bool pending_dpc;             // Offloaded onto DPC task and currently on its pending list
  isr_handler_link_t pending_isr_dpc_link;
  
  knote_list_t knote_list;      // List of knotes waiting on this ISR.
};



// Prototypes
int sys_createinterrupt(int irq, void (*callback)(int irq, struct InterruptAPI *api));
int sys_maskinterrupt(int irq);
int sys_unmaskinterrupt(int irq);
int interruptapi_mask_interrupt(int irq);
int interruptapi_unmask_interrupt(int irq);
int interruptapi_knotei(struct InterruptAPI *api, int hint);

int close_isrhandler(struct Process *proc, int fd);
struct ISRHandler *get_isrhandler(struct Process *proc, int fd);
int alloc_fd_isrhandler(struct Process *proc);
int free_fd_isrhandler(struct Process *proc, int fd);
struct ISRHandler *alloc_isrhandler(void);
void free_isrhandler(struct ISRHandler *isrhandler);
void interrupt_dpc(void);

// HAL Board Specific Functions
void enable_irq(int irq);
void disable_irq(int irq);



#endif

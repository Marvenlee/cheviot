#ifndef KERNEL_SYNC_H
#define KERNEL_SYNC_H


#include <kernel/lists.h>
#include <kernel/types.h>


struct Process;


/* @brief   Kernel condition variable for TaskSleep()/TaskWakeup() calls
 */
struct Rendez
{
  LIST(Process, blocked_list);
};


/* @brief   Kernel mutex, to eventually replace the big kernel lock on syscall entry
 */
#if 0 
struct Mutex
{
  int locked;
  struct Process *owner;
  LIST(Process, blocked_list);
};
#endif


#endif


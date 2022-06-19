#ifndef KERNEL_TIMER_H
#define KERNEL_TIMER_H

#include <kernel/lists.h>
#include <kernel/types.h>
#include <sys/time.h>

struct Filp;

#define JIFFIES_PER_SECOND 100
#define MICROSECONDS_PER_JIFFY 10000

/*
 *
 */

#define TIMER_TYPE_UNUSED 0
#define TIMER_TYPE_READY 1
#define TIMER_TYPE_SLEEP 2
#define TIMER_TYPE_ALARM 3
#define TIMER_TYPE_RELATIVE 4
#define TIMER_TYPE_ABSOLUTE 5

struct Timer {
  LIST_ENTRY(Timer) timer_entry;
  bool armed;
  long long expiration_time;
  void *arg;
  void (*callback)(struct Timer *timer);
  struct Process *process;
};

LIST_TYPE(Timer) timer_list_t;

/*
 * Prototypes
 */

int SetAlarm();

SYSCALL int sys_alarm(int seconds);
SYSCALL int sys_sleep(int seconds);

// Do away with dynamic timers.  Possibly single timer or small set of
// additional RT timers.
// Cannot epoll timers, same with interrupts?  Use signals instead?
// Add pause() syscall to wait for signals?
// Perhaps user-mode signal handlers run on their own coroutine?

//SYSCALL int CreateTimer(void);
//int DoCloseTimer(struct Filp *filp);

//struct Timer *AllocTimer(void);
//void FreeTimer(struct Timer *timer);

int SetTimeout (int milliseconds, void (*callback)(struct Timer *timer), void *arg);

void TimerTopHalf(void);
void TimerBottomHalf(void);

#endif

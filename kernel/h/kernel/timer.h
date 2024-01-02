#ifndef KERNEL_TIMER_H
#define KERNEL_TIMER_H

#include <kernel/lists.h>
#include <kernel/types.h>
#include <sys/time.h>

// Forward declarations
struct Filp;
struct Timer;

// List types
LIST_TYPE(Timer, timer_list_t, timer_list_link_t);

// Timer configuration
#define JIFFIES_PER_SECOND      100
#define MICROSECONDS_PER_JIFFY  10000
#define NANOSECONDS_PER_JIFFY   10000000


/*
 * Timer
 */
struct Timer
{
  timer_list_link_t timer_entry;
  bool armed;
  long long expiration_time;
  void *arg;
  void (*callback)(struct Timer *timer);
  struct Process *process;
};


/*
 * Prototypes
 */
int sys_alarm(int seconds);
int sys_sleep(int seconds);
int SetAlarm();
int SetTimeout (int milliseconds, void (*callback)(struct Timer *timer), void *arg);
bool softclock_trailing_hardclock(uint64_t softclock);
void TimerTopHalf(void);
void TimerBottomHalf(void);

#endif

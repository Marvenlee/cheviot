#ifndef KERNEL_TIMER_H
#define KERNEL_TIMER_H

#include <kernel/arch.h>
#include <kernel/lists.h>
#include <kernel/types.h>
#include <sys/time.h>

// Forward declarations
struct Filp;
struct Timer;

// List types
LIST_TYPE(Timer, timer_list_t, timer_list_link_t);

// Timer configuration
#define JIFFIES_PER_SECOND      100ll
#define MICROSECONDS_PER_JIFFY  10000ll
#define NANOSECONDS_PER_JIFFY   10000000ll


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
uint64_t get_hardclock(void);
void TimerTopHalf(void);
void TimerBottomHalf(void);

// Architecture-specific busy-wait sleep
int arch_spin_nanosleep(struct timespec *reg);


#endif

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
 * Timer related system calls and interrupt processing.
 *
 * Timer processing, just like interrupts is divided into two parts,
 * The timer top-half is executed within real interrupt handler when
 * an interupt arrives.  It increments the hardclock and performs
 * any adjustments to the hardware timer. The hardclock is the
 * actual system time
 *
 * The bottom half of the timer processing does the actual timer
 * expiration.  It increments a softclock variable which indicates
 * where the processing and expiration of timers has currently reached.
 * The hardclock can get ahead of the softclock if the system is particularly
 * busy or preemption disabled for long periods.
 *
 * The job of the softclock is to catch up to the hardclock and expire
 * any timers as it does so.
 *
 * A hash table is used to quickly insert and find timers to expire.  This
 * is a hashed timing wheel, with jiffies_per_second entries in the hash table.
 * This means times of 1.001, 2.001, 3.001 etc are all in the same hash
 * bucket.
 *
 * Further reading:
 *
 * 1) "Hashed and Hierarchical Timing Wheels : Data Structures for the
 *    Efficient Implementation of a Timer Facility" by G Varghese & T,Lauck
 */

//#define KDEBUG

#include <kernel/arm/raspberry.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/timer.h>
#include <kernel/types.h>
#include <sys/time.h>


/*
 * Returns the system time in seconds and microseconds.
 *
 * Acquire timer spinlock to read the current value of hardclock
 */

SYSCALL int sys_gettimeofday(struct timeval *tv_user) {
  struct timeval tv;

  SpinLock(&timer_slock);
  tv.tv_sec = hardclock_time / JIFFIES_PER_SECOND;
  tv.tv_usec = (hardclock_time % JIFFIES_PER_SECOND); // TODO: FIXME 
  SpinUnlock(&timer_slock);

  CopyOut(tv_user, &tv, sizeof(struct timeval));
  return 0;
}


/*
 * TODO:
 */
SYSCALL int sys_settimeofday(struct timeval *tv_user) {
  return 0;
}

/*
 * TODO;
 */
SYSCALL int sys_alarm(int seconds) { 
  // Enable alarm timer.
  
  return -ENOSYS;
}

/*
 *
 */
void SleepCallback(struct Timer *timer)
{
  struct Process *process = timer->process;

  TaskWakeup (&process->rendez);
}

/*
 *
 */
SYSCALL int sys_sleep(int seconds) {
  struct Process *current;
  struct Timer *timer;
  
  Info ("SysSleep(%d)", seconds);
  
  current = get_current_process();
    
  timer = &current->sleep_timer;
  
  timer->process = current;
  timer->armed = true;
  timer->callback = SleepCallback;

  SpinLock(&timer_slock);
  timer->expiration_time = hardclock_time + (seconds * JIFFIES_PER_SECOND);
  SpinUnlock(&timer_slock);

  LIST_ADD_TAIL(&timing_wheel[timer->expiration_time % JIFFIES_PER_SECOND], timer, timer_entry);

  // Can be interrupted by a signal (do we return remainder ?)

  while (timer->armed == true) {
    TaskSleep(&current->rendez);
  }
  
  return 0;
}



/*
 * Arms a timer to expire at either a relative or absolute time specified
 * by tv_user.
 * Setting tv_user to NULL will cancel an existing armed timer.
 */
int SetTimeout(int milliseconds, void (*callback)(struct Timer *), void *arg) {
  struct Timer *timer;
  struct Process *current;
  int remaining = 0;
  
  current = get_current_process();

  timer = &current->timeout_timer;
  
  DisableInterrupts();
  if (timer->armed == true) {

    remaining = hardclock_time - timer->expiration_time / JIFFIES_PER_SECOND;
    
    LIST_REM_ENTRY(&timing_wheel[timer->expiration_time % JIFFIES_PER_SECOND], timer, timer_entry);
    timer->armed = false;
  }
  EnableInterrupts();
  
  // Calculate absolute time of expiry
  
  if (milliseconds > 0) {
    SpinLock(&timer_slock);
    timer->expiration_time = hardclock_time + (milliseconds/(1000/JIFFIES_PER_SECOND)) + 1;
    SpinUnlock(&timer_slock);

    timer->process = current;    
    timer->callback = callback;
    timer->arg = arg;
    timer->armed = true;

    LIST_ADD_TAIL(&timing_wheel[timer->expiration_time % JIFFIES_PER_SECOND], timer, timer_entry);
  }

  return remaining;  
}

/*
 * Top half of timer handling.
 * Called from within timer interrupt.  Updates the quanta used of all running
 * processes and advances the hard_clock (the wall time).
 */
void TimerTopHalf(void) {
  int t;


  for (t = 0; t < max_cpu; t++) {
    if (cpu_table[t].current_process != NULL) {
      cpu_table[t].current_process->quanta_used++;
    }

    cpu_table[t].reschedule_request = true;
  }

  SpinLock(&timer_slock);
  hardclock_time++;
  SpinUnlock(&timer_slock);

  TaskWakeupFromISR(&timer_rendez);
}

/*
 * Bottom half of timer handling.
 * 
 * Run this in a high-priority kernel thread.
 */
void TimerBottomHalf(void) {
  struct Timer *timer, *next_timer;

  while (1)
  {
    KASSERT(bkl_locked == true);
    KASSERT(bkl_owner == timer_process);

    // Timer task sleeps, leaving the kernel.
    
    TaskSleep (&timer_rendez);
    
    SpinLock(&timer_slock);

    while (softclock_time < hardclock_time) {

      SpinUnlock(&timer_slock);

      timer = LIST_HEAD(&timing_wheel[softclock_time % JIFFIES_PER_SECOND]);

      while (timer != NULL) {
        next_timer = LIST_NEXT(timer, timer_entry);

        if (timer->expiration_time <= softclock_time) {
          LIST_REM_ENTRY(&timing_wheel[softclock_time % JIFFIES_PER_SECOND], timer, timer_entry);
          timer->armed = false;
  
          if (timer->callback != NULL) {
            timer->callback(timer);
          }
        }
        
        timer = next_timer;
      }

      softclock_time++;

      SpinLock(&timer_slock);
    }

    SpinUnlock(&timer_slock);
  }
}


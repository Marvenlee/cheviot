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
 * expiration. This is run as the highest priority task in the kernel.
 * The bottom half increments a softclock variable which indicates
 * where the processing and expiration of timers has currently reached.
 * The hardclock can get ahead of the softclock if the system is particularly
 * busy or preemption disabled for long periods.
 *
 * The job of the softclock is to catch up to the hardclock and expire
 * any timers as it does so.
 *
 * A hash table is used to quickly insert and find timers to expire.  This
 * is a hashed timing wheel, with jiffies_per_second entries in the hash table.
 * This means times of 1.342, 45.342, 789.342 etc are all in the same hash
 * bucket.
 *
 * Further reading:
 *
 * 1) "Hashed and Hierarchical Timing Wheels : Data Structures for the
 *    Efficient Implementation of a Timer Facility" by G Varghese & T,Lauck
 */

//#define KDEBUG 1

#include <kernel/arch.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/timer.h>
#include <kernel/types.h>
#include <sys/time.h>
#include <time.h>


/* @brief   Returns the system time in seconds and microseconds.
 * 
 * @param   tv_user, returns the system time in seconds and microseconds
 * @return  0 on success, negative errno on error
 */
int sys_gettimeofday(struct timeval *tv_user)
{
  int_state_t int_state;
  struct timeval tv;

  int_state = DisableInterrupts();
  tv.tv_sec = hardclock_time / JIFFIES_PER_SECOND;
  tv.tv_usec = (hardclock_time % JIFFIES_PER_SECOND); // TODO: FIXME  * MICROSECONDS_PER_JIFFY;
  RestoreInterrupts(int_state);

  CopyOut(tv_user, &tv, sizeof(struct timeval));
  return 0;
}


/*
 *
 */
int sys_clock_gettime(int clock_id, struct timespec *_ts)
{
	int sc;
	volatile struct timespec ts;
  int_state_t int_state;

	switch(clock_id)
	{
		case CLOCK_REALTIME:
			// TODO: return time sinc Unix epoch, Jan 1st, 1970 instead of time since boot
			int_state = DisableInterrupts();
			ts.tv_sec = hardclock_time / JIFFIES_PER_SECOND;
			ts.tv_nsec = (hardclock_time % JIFFIES_PER_SECOND) * NANOSECONDS_PER_JIFFY; 
			RestoreInterrupts(int_state);			
			sc = 0;
			break;
			
		case CLOCK_MONOTONIC:
			int_state = DisableInterrupts();
			ts.tv_sec = hardclock_time / JIFFIES_PER_SECOND;
			ts.tv_nsec = (hardclock_time % JIFFIES_PER_SECOND) * NANOSECONDS_PER_JIFFY;
			RestoreInterrupts(int_state);			
			sc = 0;
			break;
		
		case CLOCK_MONOTONIC_RAW:
			sc = arch_clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
			break;
		
		default:
		  Info("clock_gettime undefined clock_id: %d", clock_id);
			sc = -EINVAL;
	}

  if (sc != 0) {  
  	return sc;
  }

  if (CopyOut(_ts, &ts, sizeof(struct timespec)) != 0) {
  	Error("clock_gettime fault");
  	return -EFAULT;
  }
	
	return 0;
}


/* @brief   Set the system time
 *
 */
int sys_settimeofday(struct timeval *tv_user)
{
 	/* TODO:  scrap this, use clock_settime. */
  return 0;
}

/*
 *
 */
int sys_clock_settime(int clock_id, struct timespec *_ts)
{
	// TODO: clock_settime
	return -ENOSYS;
}


/* @brief   Set a timeout alarm
 * 
 * TODO;
 */
int sys_alarm(int seconds)
{ 
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


/* @brief   System call to put the current process to sleep
 * 
 * @param   seconds, duration to sleep for
 * @return  0 on success, negative errno on error 
 *
 * TODO: Abort sleep upon catching signals and return remaining time?
 */
int sys_sleep(int seconds)
{
  int_state_t int_state;
  struct Process *current;
  struct Timer *timer;
  
  current = get_current_process();
    
  timer = &current->sleep_timer;  
  timer->process = current;
  timer->armed = true;
  timer->callback = SleepCallback;

  int_state = DisableInterrupts();
  timer->expiration_time = hardclock_time + (seconds * JIFFIES_PER_SECOND);
  RestoreInterrupts(int_state);
  
  LIST_ADD_TAIL(&timing_wheel[timer->expiration_time % JIFFIES_PER_SECOND], timer, timer_entry);

  while (timer->armed == true) {
    TaskSleep(&current->rendez);
  }

  return 0;
}


int sys_nanosleep(struct timespec *_req, struct timespec *_rem)
{
  int_state_t int_state;
  struct Process *current;
  struct Timer *timer;
  struct timespec req, rem;

  current = get_current_process();
  
  if (CopyIn(&req, _req, sizeof(req)) != 0) {
	  Info ("sys_nanosleep: EFAULT");
    return -EFAULT;
  }
  
  // TODO:  spin_nanosleep() for IO processes that need to sleep for less than 10ms
//  if ((current->flags & PROCF_ALLOW_IO)) {
    if (req.tv_sec == 0 && req.tv_nsec < 10000000) {
      if (arch_spin_nanosleep(&req) == 0) {
	      return 0;
	    }
    }
//  }
      
  timer = &current->sleep_timer;  
  timer->process = current;
  timer->armed = true;
  timer->callback = SleepCallback;

  int_state = DisableInterrupts();
  
  timer->expiration_time = hardclock_time + (req.tv_sec * JIFFIES_PER_SECOND)
                                          + (req.tv_nsec / NANOSECONDS_PER_JIFFY);
  RestoreInterrupts(int_state);
  
  LIST_ADD_TAIL(&timing_wheel[timer->expiration_time % JIFFIES_PER_SECOND], timer, timer_entry);

  while (timer->armed == true) {
    TaskSleep(&current->rendez);
  }

  // TODO: if interrupted, work out remaining time and store in _rem
  Info ("sys_nanosleep awakened");
  
  return 0;
}




/* @brief   Arm a timer to expire at either a relative or absolute time
 *
 * Setting tv_user to NULL will cancel an existing armed timer.
 */
int SetTimeout(int milliseconds, void (*callback)(struct Timer *), void *arg)
{
  int_state_t int_state;
  struct Timer *timer;
  struct Process *current;
  int remaining = 0;
  
  current = get_current_process();

  timer = &current->timeout_timer;
  
  int_state = DisableInterrupts();

  if (timer->armed == true) {

    remaining = hardclock_time - timer->expiration_time / JIFFIES_PER_SECOND;
    
    LIST_REM_ENTRY(&timing_wheel[timer->expiration_time % JIFFIES_PER_SECOND], timer, timer_entry);
    timer->armed = false;
  }
  
  if (milliseconds > 0) {
    timer->expiration_time = hardclock_time + (milliseconds/(1000/JIFFIES_PER_SECOND)) + 1;
    
    timer->process = current;    
    timer->callback = callback;
    timer->arg = arg;
    timer->armed = true;

    LIST_ADD_TAIL(&timing_wheel[timer->expiration_time % JIFFIES_PER_SECOND], timer, timer_entry);
  }

  RestoreInterrupts(int_state);

  return remaining;  
}


/*
 * TODO: Replace with spinlockirqsave around hardclock fetch
 */
uint64_t get_hardclock(void)
{
  int_state_t int_state;
  uint64_t now;
  
  int_state = DisableInterrupts(); 
  now = hardclock_time;
  RestoreInterrupts(int_state);

  return now;
}


/* @brief   Timer handling in the timer interrupt service routine
 *
 * Called from within timer interrupt.  Updates the quanta used of all currently
 * running processes and advances the hard_clock (the wall time).
 *
 * NOTES: For SMP, we may have separate hardware timers for each CPU and use
 * these to increment the quanta used for scheduling. Only a single CPU needs
 * to increment the hardclock time. 
 */
void TimerTopHalf(void)
{
  int_state_t int_state;
  
  for (int t = 0; t < max_cpu; t++) {
    if (cpu_table[t].current_process != NULL) {
      cpu_table[t].current_process->quanta_used++;
    }

//    cpu_table[t].reschedule_request = true;     // Unused,
  }

//  int_state = DisableInterrupts(); 		// FIXME: Nested interrupts not supported, not needed
  hardclock_time++;
//  RestoreInterrupts(int_state);
  
  TaskWakeupFromISR(&timer_rendez);
}


/* @brief   Timer handling deferred to a high priority kernel task.
 *
 * NOTES:
 * Is there a race condition if TaskSleep cancels timer and bucket is being processed ?
 * It is removed from timing wheel, no longer armed but callback still called.
 * We have the BKL/cooperative scheduling in kernel, only single task is running until
 * Reschedule() is called.  TaskWakeup() called from callbacks such as sleep do not reschedule.
 */
void TimerBottomHalf(void)
{
  int_state_t int_state;
  bool logtimerwakeup = false;
  
  Info("TimerBottomHalf: hard_clock =%u",(uint32_t)hardclock_time);
  
  struct Timer *timer, *next_timer;

  while (1) {
    KASSERT(bkl_locked == true);
    KASSERT(bkl_owner == timer_process);
    
    TaskSleep(&timer_rendez);
    
    int_state = DisableInterrupts(); 
    while (softclock_time < hardclock_time) {
      RestoreInterrupts(int_state);
      
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

      int_state = DisableInterrupts();
    }
    RestoreInterrupts(int_state);
    
    // What if timer interrupt occurs here, does this mean we end up waiting for
    // an extra JIFFY for the next timer interrupt?
  }
}


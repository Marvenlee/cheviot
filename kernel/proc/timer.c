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

#include <kernel/types.h>
#include <kernel/proc.h>
#include <kernel/timer.h>
#include <kernel/error.h>
#include <kernel/dbg.h>
#include <kernel/globals.h>




/*
 * Returns the system time in seconds and microseconds.
 *
 * Acquire timer spinlock to read the current value of hardclock
 */

SYSCALL int GetSystemTime (struct TimeVal *tv_user)
{
    struct TimeVal tv;


    DisableInterrupts();
    SpinLock (&timer_slock);
    tv.seconds = hardclock_seconds;
    tv.microseconds = hardclock_jiffies * 1000000 / JIFFIES_PER_SECOND;
    SpinUnlock (&timer_slock);
    EnableInterrupts();

    CopyOut (tv_user, &tv, sizeof (struct TimeVal));
    return 0;
}




/*
 * Creates a timer kernel object and returns an integer handle.
 * The timer may be armed at a later time with SetTimer().
 */

SYSCALL int CreateTimer(void)
{
    struct Timer *timer;
    struct Process *current;
    int handle;
        
    current = GetCurrentProcess();
    
    if (free_handle_cnt < 1 || free_timer_cnt < 1)
        return resourceErr;
    
    DisablePreemption();
    
    handle = AllocHandle();

    timer = LIST_HEAD (&free_timer_list);
    LIST_REM_HEAD (&free_timer_list, timer_entry);
    free_timer_cnt --;
    
    timer->armed = FALSE;
    timer->type = 0;
    timer->expiration_seconds = 0;
    timer->expiration_jiffies = 0;
    timer->handle = handle;
    
    SetObject (current, handle, HANDLE_TYPE_TIMER, timer);
    
    return handle;
}




/*
 * Called by ClosePendingHandles() to free a timer.
 */
 
int DoCloseTimer(int handle)
{
    struct Timer *timer;
    struct Process *current;
    
    current = GetCurrentProcess();
        
    if ((timer = GetObject(current, handle, HANDLE_TYPE_TIMER)) == NULL)
        return paramErr;

    DisablePreemption();

    if (timer->armed == TRUE)
    {
        LIST_REM_ENTRY(&timing_wheel[timer->expiration_jiffies], timer, timer_entry);
        timer->armed = FALSE;
    }
    
    LIST_ADD_HEAD (&free_timer_list, timer, timer_entry);
    free_timer_cnt ++;
    
    FreeHandle(handle);
    
    return 0;
}




/*
 * Arms a timer to expire at either a relative or absolute time specified
 * by tv_user.
 * Setting tv_user to NULL will cancel an existing armed timer.
 */

SYSCALL int SetTimer (int handle, int type, struct TimeVal *tv_user)
{
    struct Timer *timer;
    struct Process *current;
    struct TimeVal tv;

        
    current = GetCurrentProcess();
    CopyIn (&tv, tv_user, sizeof(struct TimeVal));
    
    
    if ((timer = GetObject(current, handle, HANDLE_TYPE_TIMER)) == NULL)
        return paramErr;


    DisablePreemption();

    if (tv_user != NULL)
    {
        if (timer->armed == TRUE)
        {
            LIST_REM_ENTRY(&timing_wheel[timer->expiration_jiffies], timer, timer_entry);
            timer->armed = FALSE;
        }
        
        DoClearEvent(current, handle);
                        
        if (type == TIMER_TYPE_RELATIVE)
        {
            /* CHECKME: Unsure if RELATIVE timers expire at correct time */
        
            timer->expiration_seconds = tv.seconds + (tv.microseconds/1000000);
            timer->expiration_jiffies = (((tv.microseconds % 1000000)
                                    / MICROSECONDS_PER_JIFFY) + hardclock_jiffies)
                                    % JIFFIES_PER_SECOND;
        }
        else
        {
            timer->expiration_seconds = tv.seconds + (tv.microseconds/1000000);
            timer->expiration_jiffies = (((tv.microseconds % 1000000)
                                    / MICROSECONDS_PER_JIFFY))
                                    % JIFFIES_PER_SECOND;
        }
            
        timer->type = type;
        timer->armed = TRUE;
        
        LIST_ADD_TAIL(&timing_wheel[timer->expiration_jiffies], timer, timer_entry);
        
        return 0;
    }
    else
    {   
        if (timer->armed == TRUE)
        {
            LIST_REM_ENTRY(&timing_wheel[timer->expiration_jiffies], timer, timer_entry);
            timer->armed = FALSE;
        }
        
        DoClearEvent(current, handle);
        
        return 0;
    }
}




/*
 * Top half of timer handling.
 * Called from within timer interrupt.  Updates the quanta used of all running
 * processes and advances the hard_clock (the wall time).
 */

void TimerTopHalf (void)
{
    int t;
    
    for (t=0; t< max_cpu; t++)
    {
        cpu_table[t].current_process->quanta_used++;
    }

    reschedule_request = TRUE;

    DisableInterrupts();
    SpinLock (&timer_slock);
 
    hardclock_jiffies ++;
    
    if (hardclock_jiffies == JIFFIES_PER_SECOND)
    {
        hardclock_jiffies = 0;
        hardclock_seconds ++;
    }
    
    SpinUnlock (&timer_slock);
    EnableInterrupts();
}




/*
 * Bottom half of timer handling.
 * Called within the KernelExit() code with preemption disabled.
 */

void TimerBottomHalf (void)
{
    struct Timer *timer, *next_timer;   


    KASSERT (inkernel_lock == 1);
    

    DisableInterrupts();
    SpinLock (&timer_slock);            

    while (softclock_seconds < hardclock_seconds || softclock_jiffies < hardclock_jiffies)
    {
        SpinUnlock (&timer_slock);
        EnableInterrupts();
    
        timer = LIST_HEAD (&timing_wheel[softclock_jiffies]);
        
        while (timer != NULL)
        {
            next_timer = LIST_NEXT (timer, timer_entry);
  
            KASSERT (timer->type == TIMER_TYPE_RELATIVE
                || timer->type == TIMER_TYPE_ABSOLUTE);
            
            switch (timer->type)
            {
                case TIMER_TYPE_RELATIVE:
                    if (timer->expiration_seconds == 0)
                    {                   
                        LIST_REM_ENTRY(&timing_wheel[softclock_jiffies], timer,
                            timer_entry);
                        
                        timer->armed = FALSE;
                        DoRaiseEvent (timer->handle);
                    }
                    else
                    {
                        timer->expiration_seconds--;
                    }
                    
                    break;
                
                case TIMER_TYPE_ABSOLUTE:
                    if (timer->expiration_seconds == softclock_seconds)
                    {
                        LIST_REM_ENTRY(&timing_wheel[softclock_jiffies], timer,
                            timer_entry);
                            
                        timer->armed = FALSE;
                        DoRaiseEvent (timer->handle);
                    }
                                        
                    break;
                
                default:
                    break;
            }
            
            timer = next_timer;
        }
            
                    
        softclock_jiffies ++;
        
        if (softclock_jiffies == JIFFIES_PER_SECOND)
        {
            softclock_jiffies = 0;
            softclock_seconds ++;
        }
        
        DisableInterrupts();
        SpinLock (&timer_slock);
    }

    SpinUnlock (&timer_slock);    
    EnableInterrupts(); 
}




#ifndef KERNEL_TIMER_H
#define KERNEL_TIMER_H

#include <kernel/types.h>
#include <kernel/lists.h>





#define JIFFIES_PER_SECOND              100
#define MICROSECONDS_PER_JIFFY          10000



/*
 *
 */

#define TIMER_TYPE_RELATIVE             0
#define TIMER_TYPE_ABSOLUTE             1



/*
 *
 */


struct TimeVal
{
    long long seconds;
    long microseconds;
};





struct Timer
{
    LIST_ENTRY (Timer) timer_entry;
    bool armed;
    int type;
    long long expiration_seconds;
    long expiration_jiffies;
    int handle;
};




LIST_TYPE (Timer) timer_list_t;






/*
 * Prototypes
 */

void SetWatchdog (int seconds);
int CheckWatchdog (void);





SYSCALL int CreateTimer (void);
int DoCloseTimer (int handle);

struct Timer *AllocTimer(void);
void FreeTimer (struct Timer *timer);
SYSCALL int SetTimer (int timer_id, int type, struct TimeVal *tv_user);


void TimerTopHalf(void);
void TimerBottomHalf(void);



#endif


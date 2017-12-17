#include "types.h"
#include "dbg.h"
#include "root.h"
#include "module.h"
#include "sys/syscalls.h"

/*!
*/
void RootMain (void)
{
    int timer;
    struct TimeVal tv;
    int pid;
    int status;
    int t;
    
    Debug ("Root Hello World!");
    
    //GetSystemTime (struct TimeVal *tv_user);
    
    tv.seconds = 10;
    tv.microseconds = 0;
    
    timer = CreateTimer();
    
    if (timer == -1)
    {
        Debug (" ****** FAILED TO CREATE TIMER ******");
    }

    while (1)
    {
        for (t=0; t<10; t++)
        {
            if (SetTimer (timer, TIMER_TYPE_RELATIVE, &tv) == -1)
            {
                Debug ("Failed to set timer");
            }

            WaitEvent (timer);
            
            Debug ("Hello again!");
        }
        
        pid = Fork();

        if (pid == -1)
        {
            Debug ("Failed to Fork");
        }
        else if (pid == 0)
        {
            Debug ("Hello from child process");
            Exit (0);
            Debug ("Child process failed to exit");
        }
        else
        {
            Debug ("Parent calling Join");
            WaitPid (pid, &status);
            Debug ("Join complete");
        }
    }
        
    Debug ("Shouldn't get here");
    while(1);
}




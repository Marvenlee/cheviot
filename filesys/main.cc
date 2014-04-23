#include <sys/types.h>
#include <sys/syscalls.h>
#include <interfaces/filesystem.h>
#include <sys/debug.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include "filesystem.h"
#include <new> 



/*
 *
 */

void taskmain (int argc, char *argv[])
{
    int h;
    int exit_status = 0;
    
    
    // Initialize Executive,  create any daemon-like coroutine tasks, load drivers etc.
    // AddEventRendez (handle_xyz, &rendez);  for each handle.
        
	while (exit_status == 0)
	{	
		h = WaitFor(-1);
		
		if (h >= 0)
		{
            // WakeupEventRendez (h);

		    // Wakes up approriate task for this handle 
    	}
		
		while (taskyield() != 0);
    }
    
    // DeleteEventRendez (handle_xyz);
}





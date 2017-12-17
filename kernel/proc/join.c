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
 * Function to join a process and return its exit status.
 */

#include <kernel/types.h>
#include <kernel/proc.h>
#include <kernel/dbg.h>
#include <kernel/utility.h>
#include <kernel/error.h>
#include <kernel/lists.h>
#include <kernel/globals.h>



/*
 * Joins an already exited process, returning its status and performing
 * final cleanup and release of its handle.
 */

SYSCALL int WaitPid (int handle, int *status)
{
    struct Process *child;
    struct Process *current;

    current = GetCurrentProcess();
    
    if ((child = GetObject (current, handle, HANDLE_TYPE_PROCESS)) == NULL)
        return paramErr;
        
    if (status != NULL)
        CopyOut (status, &child->exit_status, sizeof (int));
    
    DisablePreemption();

    if (child->state != PROC_STATE_ZOMBIE)
        return paramErr;
    
    PmapDestroy (&child->as);
    
    ArchFreeProcess (child);
    FreeProcess (child);
    FreeHandle (handle);
    
    return 0;
}


/*
 * Closes a process when called by the app calls CloseHandle().
 *
 * Sets the owner to the root process, does not block and does not
 * try to kill the process.
 *
 * Makes child process an orphaned process.
 *
 * If called by root then performs the Join() cleanup of zombie process.
 */
 
int DoCloseProcess (int h)
{
	struct Process *child;
	struct Process *current;
	
		
	current = GetCurrentProcess();
	
	if ((child = GetObject (current, h, HANDLE_TYPE_PROCESS)) == NULL)
	    return paramErr;
	
	DisablePreemption();

    if (child->state == PROC_STATE_ZOMBIE)
    {
        ArchFreeProcess (child);
        FreeAddressSpace (&child->as);
        FreeProcess (child);
        FreeHandle (h);
        return 0;
    }
	else
	{
		child->parent = NULL;
		
		// Something needs to reap these processes.
		// Possible in continuation.
		
		// send sig_hangup event  ???????
		// FIXME:  use negative error  sighangupErr
		
		FreeHandle (h);
		return 0;
	}
}




SYSCALL int Kill (int pid)
{
	// Will probably only allow root to handle kill syscall.
	// Sessions will create a namespace handle /session to process manager
	// allowing any process in session with handle to kill named processes in session.
	return 0;
}

/*
 *
 */

int TerminateProcess (int h)
{
	struct Process *child;
	struct Process *current;

	
	current = GetCurrentProcess();
	
	if ((child = GetObject (current, h, HANDLE_TYPE_PROCESS)) == NULL)
	    return paramErr;
			
	DisablePreemption();

// FIXME:  Use negative event handles for signals?    
//	replace with termErr
//    DoRaiseEvent (child->sigterm_handle);

    return 0;
}









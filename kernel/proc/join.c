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
#include <kernel/error.h>
#include <kernel/lists.h>
#include <kernel/globals.h>



/*
 * Joins an already exited process, returning its status and performing
 * final cleanup and release of its handle.
 */

int Join (int handle, int *status)
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
	struct Handle *handle;
	
	
	current = GetCurrentProcess();
	
	if ((child = GetObject (current, h, HANDLE_TYPE_PROCESS)) == NULL)
	    return paramErr;
	
	DisablePreemption();

    handle = FindHandle (current, h);
    
    if (child->state == PROC_STATE_ZOMBIE)
    {
        ArchFreeProcess (child);
        FreeProcess (child);
        FreeHandle (h);
        return 0;
    }
    else if (handle->owner != reaper_task)
    {
        DoRaiseEvent (child->sighangup_handle);
        handle->owner = reaper_task;
        return 0;
    }
    else
    {
        return handleErr;
    }
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
    
    DoRaiseEvent (child->sigterm_handle);

    return 0;
}









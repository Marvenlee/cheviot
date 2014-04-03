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
 * Functions to close a process.
 */
 
#include <kernel/types.h>
#include <kernel/proc.h>
#include <kernel/error.h>
#include <kernel/dbg.h>
#include <kernel/globals.h>




/*
 * Closes a process.  Sets the TSF_KILL flag in child process.
 * Called from within ClosePendingHandles() within KernelExit().
 * This function blocks until the process exits.  As it blocks
 * it will restart from the beginning the next time the process
 * is scheduled to run from within KernelExit.
 */
 
int DoCloseProcess (int h)
{
	struct Process *child;
	struct Process *current;
	
	current = GetCurrentProcess();
	
	if ((child = GetObject (current, h, HANDLE_TYPE_PROCESS)) == NULL)
	    return paramErr;
	
	KASSERT (child != current);
		
	DisablePreemption();

	child->task_state.flags |= TSF_KILL;

	if (child->state != PROC_STATE_ZOMBIE)
		WaitFor (h);
	
	ArchFreeProcess (child);
	FreeProcess (child);
	FreeHandle (h);
	
			
	return 0;
}



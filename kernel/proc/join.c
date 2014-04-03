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




/*
 * Joins an already exited process, returning its status and performing
 * final cleanup and release of its handle.
 *
 * FIXME: CHECKME:  Should Join() block and should the handle be placed
 * on the close_handle_list ???
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



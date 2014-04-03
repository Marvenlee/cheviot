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
 * Functions and system calls for handle management
 */

#include <kernel/types.h>
#include <kernel/proc.h>
#include <kernel/dbg.h>
#include <kernel/error.h>
#include <kernel/globals.h>







/*
 * CloseHandle() is the single system call used to close any kernel resource.
 */

SYSCALL int CloseHandle(int h)
{
	struct Process *current;
	
	current = GetCurrentProcess();
		
	if (h < 0 || h >= max_handle || handle_table[h].owner != current)
		return paramErr;

	DisablePreemption();
	
	LIST_ADD_TAIL (&current->close_handle_list, &handle_table[h], link);
    return 0;
}




/*
 * Closes any handles placed on the process's close_handle_list when a
 * Channel is closed.  As Channels are closed any embedded handles in
 * the message segments are appended to this list.
 */
 
void ClosePendingHandles (void)
{
    int h;
    struct Handle *handle;
    struct Process *current;
    
    current = GetCurrentProcess();
    
    DisablePreemption();
	
	while ((handle = LIST_HEAD(&current->close_handle_list)) != NULL)
	{
	    LIST_REM_HEAD (&current->close_handle_list, link);
	    
	    h = handle - handle_table;
	
    	switch (handle->type)
    	{
    		case HANDLE_TYPE_PROCESS:
    			DoCloseProcess (h);
    			break;
    			
    		case HANDLE_TYPE_ISR:
    			DoCloseInterruptHandler (h);
    			break;
    						
    		case HANDLE_TYPE_CHANNEL:
    			DoCloseChannel (h);
    			break;
    			
    		case HANDLE_TYPE_TIMER:
    			DoCloseTimer (h);
                break;
                
    		case HANDLE_TYPE_CONDITION:
    			DoCloseCondition (h);
    			break;
    
    		default:
    			break;
    	}
	}
}








/*
 * CloseHandle() is the single system call used to close any kernel resource.
 */

/*
SYSCALL int CloseHandle (int h)
{
    struct Process *current;
    int result;
    
    
    current = GetCurrentProcess();
        
    if (h <= 0 || h >= max_handle)
        return paramErr;
    
    if (handle_table[h].owner != current)
        return paramErr;
    
    switch (handle_table[h].type)
    {
        case HANDLE_TYPE_PROCESS:
            result = DoCloseProcess (h);
            break;
            
        case HANDLE_TYPE_ISR:
            result = DoCloseInterruptHandler (h);
            break;
                        
        case HANDLE_TYPE_CHANNEL:
            result = DoCloseChannel (h);
            break;
            
        case HANDLE_TYPE_TIMER:
            result = DoCloseTimer (h);
            break;
            
        case HANDLE_TYPE_CONDITION:
            result = DoCloseCondition (h);
            break;

        default:
            result = paramErr;
            break;
    }
    
    return result;
}
*/



/*
 * Returns a pointer to the object referenced by the handle 'h'.  Ensures
 * it belongs to the specified process and is of the requested type.
 */

void *GetObject(struct Process *proc, int h, int type)
{
    if (h < 0 || h >= max_handle)
        return NULL;

    if (handle_table[h].owner != proc || handle_table[h].type != type)
        return NULL;
    
    KASSERT (handle_table[h].object != NULL);
    
    return handle_table[h].object;
}




/*
 * Returns a pointer to the Handle structure for the given handle 'h'.
 * Ensures it is within range and belongs to the specified process.
 */


struct Handle *GetHandle (struct Process *proc, int h)
{
    if (h < 0 || h >= max_handle)
        return NULL;

    if (handle_table[h].owner != proc)
        return NULL;
    
    return &handle_table[h];
}




/*
 * Initializes a Handle structure for the given handle 'h'.  Initializes
 * the owner process, the type and the pointer to the kernel object. 
 */

void SetObject (struct Process *proc, int h, int type, void *object)
{
    KASSERT (0 <= h && h < max_handle);
        
    handle_table[h].type = type;
    handle_table[h].object = object;
    handle_table[h].owner = proc;
    handle_table[h].pending = 0;
}




/*
 * Returns the integer number of the next handles that will be allocated with
 * AllocHandle().  Used by system calls such as CreateChannel() that need to
 * return more than one handle to the user process before DisablePreemption()
 * is called.  From that point on CopyOut() cannot be called to return the
 * handles.
 */

int PeekHandle (int index)
{
    int t;
    struct Handle *handle;  
    
    handle = LIST_HEAD (&free_handle_list);
    
    for (t=0; t <= index; t++)
    {
        handle = LIST_NEXT (handle, link);
    }
    
    return handle - handle_table;
    
}




/*
 * Allocates a handle structure.  Checks to see that free_handle_cnt is
 * non-zero should already have been performed prior to calling AllocHandle().
 */

int AllocHandle (void)
{
    struct Handle *handle;  
    
    handle = LIST_HEAD (&free_handle_list);
    
    KASSERT (handle != NULL);
    
    free_handle_cnt --;
    
    KASSERT (free_handle_cnt >= 0);
    
    LIST_REM_HEAD (&free_handle_list, link);
    
    return handle - handle_table;
}




/*
 * Returns a handle to the free handle list and increments the free_handle_cnt.
 */

void FreeHandle (int h)
{
    KASSERT (0 <= h && h < max_handle);

    DoClearEvent (handle_table[h].owner, h);

    free_handle_cnt ++; 
    LIST_ADD_TAIL (&free_handle_list, &handle_table[h], link);
    handle_table[h].type = HANDLE_TYPE_FREE;
    handle_table[h].owner = NULL;
}




/*
 * Grants a handle to a recipient process.  The recipient can either
 * be determined from a channel or process handle.
 *
 * The original intent was to pass handles between processes by embedding
 * them into a message segment using InjectHandle (mem, handle) and
 * handle = ExtractHandle (mem).  The process could then pass a message
 * segment pointed to by 'mem' to another process which would then extract
 * the handle.
 *
 * It got messy and could potentially form loops or dead ends, passing
 * one channel handle to then other end.
 *
 * Problem is this works fine for servers, it can grant a handle and then
 * send the handle number in the process message.  The client trusts
 * whatever handle number in the message is correct.
 *
 * What if we want to grant a handle to a server or other process?
 * 
 * *** Should a granted handle raise an event?
 *
 * OR can we somehow send a Grant as a message???  Maybe check queue
 * is empty????????????
 * 
 */

/*
SYSCALL int GrantHandle (int r, int h, bits32_t flags)
{
    struct Process *current;
    struct Process *recipient;
    struct Channel *channel;

    current = GetCurrentProcess();

    if (h < 0 || h >= max_handle || handle_table[h].owner != current)
        return paramErr;
       
    if (r < 0 || r >= max_handle || handle_table[r].owner != current)
        return paramErr;
    
    if (handle_table[r].type == HANDLE_TYPE_PROCESS)
    {
        recipient = handle_table[r].object;
    }
    else if (handle_table[r].type == HANDLE_TYPE_CHANNEL)
    {
        channel = handle_table[r].object;
        
        if (channel->process[0] == current)
            recipient = channel->process[1];
        else if (channel->process[1] == current)
            recipient = channel->process[1];
        else
            return paramErr;
    }

    if (recipient == NULL)
        return paramErr;
    
    if (handle_table[h].flags & HANDLEF_GRANT_ONCE)
        return privilegeErr;
    
    DisablePreemption();
    
    handle_table[h].flags = flags;
    handle_table[h].owner = recipient;
    return 0;
}
*/




















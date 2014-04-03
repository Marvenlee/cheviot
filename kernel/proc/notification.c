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
 * WITHOUT WARRANTIES OR notifITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * 'Notification' interprocess communication system calls and functions.
 *
 * A 'Notification' is a form of state notification.  Similar to the
 * a message passing 'Channel' or pipe. It is used to inform the
 * recipient of a change in state.
 *
 * A call to CreateNotification creates a Notification within the
 * kernel and returns two handles to the Notification in a similar 
 * fashion to the Unix pipe() system call.
 *
 * A process can notify another process of an event by calling
 * PutNotification().  The other process can then read the state by
 * call GetNotification().
 *
 * Each end of a Notification has its own unique state variable.
 */

#include <kernel/types.h>
#include <kernel/lists.h>
#include <kernel/proc.h>
#include <kernel/dbg.h>
#include <kernel/utility.h>
#include <kernel/error.h>
#include <kernel/globals.h>




/*
 * Sets the state of a 'Notification' object and raises an event on
 * the other handle to indicate the state may have changed.
 *
 * PutNotification does not block. Multiple PutNotification calls are not
 * queued so only the current state is retrieved by GetNotification 
 *
 * Useful for notifying a client about changes to objects that the server
 * manages such as files or processes.
 */

SYSCALL int PutNotification (int h, int value)
{
    struct Notification *notif;
    struct Process *current;
    int q;
    
    
    current = GetCurrentProcess();

    if ((notif = GetObject(current, h, HANDLE_TYPE_NOTIFICATION)) == NULL)
        return paramErr;
    
    q = (h == notif->handle[0]) ? 1 : 0;
    
    if (notif->handle[q] == -1)
        return connectionErr;

    DisablePreemption();
    
    notif->state = value;
    DoRaiseEvent (notif->handle[q]);
    
    return 0;
}




/*
 * Gets the current state of the Notification object
 */

SYSCALL int GetNotification (int h)
{
    struct Notification *notif;
    struct Process *current;
    int q;
    int value;
    
    
    current = GetCurrentProcess();
    
    if ((notif = GetObject(current, h, HANDLE_TYPE_NOTIFICATION)) == NULL)
        return paramErr;
    
    q = (h == notif->handle[0]) ? 0 : 1;
    
    DisablePreemption();
        
    value = notif->state;
    DoClearEvent (current, h);
        
    return value;
}





/*
 * Creates a Notification for message passing and returns two handles used as
 * endpoints.  Similar to Unix pipe(). These handles can then be passed
 * to other processes by embedding them in messages.
 */

SYSCALL int CreateNotification (int result[2])
{
    int handle[2];
    struct Notification *notif;
    struct Process *current;
    
    current = GetCurrentProcess();

    if (free_handle_cnt < 2 || free_Notification_cnt < 1)
        return resourceErr;

    handle[0] = PeekHandle(0);
    handle[1] = PeekHandle(1);
    
    CopyOut (result, handle, sizeof (handle));

    DisablePreemption();
                
    handle[0] = AllocHandle();
    handle[1] = AllocHandle();
        
    notif = LIST_HEAD(&free_Notification_list);
    LIST_REM_HEAD (&free_Notification_list, link);
    free_Notification_cnt --;
        
    notif->handle[0] = handle[0];
    notif->handle[1] = handle[1];
    notif->state = 0;
    
    SetObject (current, handle[0], HANDLE_TYPE_NOTIFICATION, notif);
    SetObject (current, handle[1], HANDLE_TYPE_NOTIFICATION, notif);

    return 0;
}




/*
 * Called by CloseHandle() to close one end of a Notification.  If both ends are
 * freed then the Notification is deleted.
 */

int DoCloseNotification (int h)
{
    struct Notification *notif;
    struct Process *current;
    int u, v;
    
    
    current = GetCurrentProcess();
    
    if ((notif = GetObject (current, h, HANDLE_TYPE_NOTIFICATION)) == NULL)
        return paramErr;
            
    u = (notif->handle[0] == h) ? 0 : 1;  
    v = (notif->handle[1] == h) ? 1 : 0;
    
    DisablePreemption();
        
    notif->handle[u] = -1;
    
    if (notif->handle[v] == -1)
    {
        LIST_ADD_HEAD (&free_Notification_list, notif, link);
        free_Notification_cnt ++;
    }
    else
    {
        DoRaiseEvent (notif->handle[v]);
    }
    
    FreeHandle (h);
    return 0;
}






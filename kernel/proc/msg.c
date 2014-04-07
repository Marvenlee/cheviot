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
 * Message passing system calls and channel creation functions.
 */

#include <kernel/types.h>
#include <kernel/lists.h>
#include <kernel/proc.h>
#include <kernel/dbg.h>
#include <kernel/utility.h>
#include <kernel/error.h>
#include <kernel/globals.h>

 

 
/*
 * Sends a segment of memory through a channel to the recipient process.
 *
 * PutMsg() is a non blocking call and queues the segments on the recipients
 * end of the channel.
 *
 * struct MyMessage *msg;
 * struct MyMessageReply *reply;
 * ssize_t reply_sz;
 *
 * while (1)
 * {
 *     // initialize message segment
 *     iov[0] = message_segment;
 *     PutMsg (handle, msg);
 *     WaitFor (handle);
 *     reply_sz = GetMsg (handle, &reply, PROT_READWRITE);
 *     // process reply
 * }
 */

SYSCALL int PutMsg (int port_h, void *msg, bits32_t flags)
{
    struct Process *current;
    struct VirtualSegment *vs;
    struct Channel *channel;
    struct Parcel *parcel;
    int rq;
 

    current = GetCurrentProcess();
        
    vs = VirtualSegmentFind ((vm_addr)msg);
    
    if (vs == NULL || vs->owner != current || (vs->flags & MEM_MASK) != MEM_ALLOC)
        return memoryErr;
    
    if ((channel = GetObject(current, port_h, HANDLE_TYPE_CHANNEL)) == NULL)
        return paramErr;
    
    if (port_h == channel->handle[0])
        rq = 1;
    else
        rq = 0;
    
    if (channel->handle[rq] == -1)
        return connectionErr;

    DisablePreemption();
    
    parcel = LIST_HEAD (&free_parcel_list);
    LIST_REM_HEAD (&free_parcel_list, link);
    
    KASSERT (parcel != NULL);
    
    PmapRemoveRegion(&current->pmap, vs);
    
    parcel->type = PARCEL_MSG;
    parcel->content.virtualsegment = vs;
    LIST_ADD_TAIL (&channel->msg_list[rq], parcel, link);
    
    vs->parcel = parcel;
    vs->owner = NULL;

    if (!(flags & MSG_SILENT))
        DoRaiseEvent (channel->handle[rq]);
    
    return 0;
}




/*
 * A channel is a bidrectional communication pipe that allows processes to
 * send memory segments to one another.
 *
 * Gets a message from the receiving queue of the channel and returns
 * a pointer to the segment and its size.  Ensures that the correct access
 * permissions are available for this message.   If the permissions
 * are not correct GetMsg() returns messageErr and leaves the message in
 * queue so that the process can either call GetMsg() again with the correct
 * access permissions or call CloseHandle() to free the port and messages.
 *
 * 0 is returned with no errno if there are no messages in the queue.  If
 * the channel receive queue is empty has been closed on the other end then
 * it returns connectionErr.
 *
 * If the segment is currently marked as busy, we don't update the page tables.
 * We let it be caught by the page fault handler which will sleep until it
 * is unbusied.
 */

SYSCALL ssize_t GetMsg (int port_h, void **rcv_msg, bits32_t access)
{
    struct Channel *channel;
    struct Process *current;
    struct VirtualSegment *vs;
    struct Parcel *parcel;
    int rq;
    
    
    current = GetCurrentProcess();
    
    if ((channel = GetObject(current, port_h, HANDLE_TYPE_CHANNEL)) == NULL)
        return paramErr;

    if (channel->handle[0] == -1 || channel->handle[1] == -1)
        return connectionErr;
    
    if (port_h == channel->handle[0])
        rq = 0;
    else
        rq = 1;
        
    parcel = LIST_HEAD (&channel->msg_list[rq]);
   
    if (parcel == NULL)
        return 0;
         
    if (parcel->type != PARCEL_MSG)
        return messageErr;
        
    vs = parcel->content.virtualsegment;
        
    if ((vs->flags & access & PROT_MASK) != (access & PROT_MASK))
        return privilegeErr;
    
    CopyOut (rcv_msg, &vs->base, sizeof (void *));
    
    DisablePreemption();
    
    LIST_REM_HEAD (&channel->msg_list[rq], link);
    
    LIST_ADD_HEAD (&free_parcel_list, parcel, link);
        
    vs->parcel = NULL;
    vs->owner = current;
    
    
    // possibly move the busy flags into vs
    
    if (vs->busy == FALSE)
        PmapEnterRegion (&current->pmap, vs, vs->base);
    
    return vs->ceiling - vs->base;
}




/*
 *
 */
 
SYSCALL ssize_t GetNextMsgType (int port_h)
{
    struct Channel *channel;
    struct Process *current;
    struct Parcel *parcel;
    int rq;
    

    current = GetCurrentProcess();
    
    if ((channel = GetObject(current, port_h, HANDLE_TYPE_CHANNEL)) == NULL)
        return paramErr;

    if (channel->handle[0] == -1 || channel->handle[1] == -1)
        return connectionErr;
    
    if (port_h == channel->handle[0])
        rq = 0;
    else
        rq = 1;
        
    parcel = LIST_HEAD (&channel->msg_list[rq]);
   
    if (parcel == NULL)
        return messageErr;
         
    return parcel->type;
}


/*
 * Creates a channel for message passing and returns two handles used as
 * endpoints.  Much like Unix pipe() system calls.  These handles can be
 * passed to other processes by embedding them in messages using the
 * InjectHandle and ExtractHandle system calls.
 */

SYSCALL int CreateChannel (int result[2])
{
    int handle[2];
    struct Channel *channel;
    struct Process *current;

        
    current = GetCurrentProcess();

    if (free_handle_cnt < 2 || free_channel_cnt < 1)
        return resourceErr;

    handle[0] = PeekHandle(0);
    handle[1] = PeekHandle(1);
    
    CopyOut (result, handle, sizeof (handle));

    DisablePreemption();
                
    handle[0] = AllocHandle();
    handle[1] = AllocHandle();
        
    channel = LIST_HEAD(&free_channel_list);
    LIST_REM_HEAD (&free_channel_list, link);
    free_channel_cnt --;
        
    channel->handle[0] = handle[0];
    channel->handle[1] = handle[1];
    
    LIST_INIT (&channel->msg_list[0]);
    LIST_INIT (&channel->msg_list[1]);
    
    SetObject (current, handle[0], HANDLE_TYPE_CHANNEL, channel);
    SetObject (current, handle[1], HANDLE_TYPE_CHANNEL, channel);

    return 0;
}




/*
 * Called by CloseHandle() to close one end of a channel.  If both ends are
 * freed then the channel structure is deleted.
 *
 * If either of handles to a Channel are closed then all of the queued messages
 * going in both directions are freed.  This will close and delete the channel
 * if a message is sent to the channel with one of the channel's handles
 * embedded in the message.
 */

int DoCloseChannel (int h)
{
    struct Channel *ch;
    struct Process *current;
    struct VirtualSegment *vs;
    struct Parcel *parcel;
    int t, t2;
    
    
    current = GetCurrentProcess();
    
    if ((ch = GetObject (current, h, HANDLE_TYPE_CHANNEL)) == NULL)
        return paramErr;
            
    DisablePreemption();
    
    for (t=0; t<2; t++)
    {
        while ((parcel = LIST_HEAD (&ch->msg_list[t])) != NULL)
        {
            if (parcel->type == PARCEL_MSG)
            {
                vs = parcel->content.virtualsegment;
                vs->owner = current;
            
                VirtualFree (vs->base);
            }
            else if (parcel->type == PARCEL_HANDLE);
            {
                CloseHandle (parcel->content.handle);
            }
            
            LIST_REM_HEAD (&ch->msg_list[t], link);            
        }
                    
        t2 = (t + 1) % 2;

        if (ch->handle[t2] == -1)
        {
            LIST_ADD_HEAD (&free_channel_list, ch, link);
            free_channel_cnt ++;
        }
        else
        {
            DoRaiseEvent (ch->handle[t2]);
        }
    }


    FreeHandle (h);
    
    return 0;
}




/*
 * Peforms a check to see if handles h1 and h2 are ports to the same
 * Channel.  Both handles must be owned by the current process.
 */
 
SYSCALL int IsAChannel (int h1, int h2)
{
    struct Process *current;
    struct Channel *ch1, *ch2;
    
    current = GetCurrentProcess();
    
    if (h1 == h2)
        return paramErr;

    if ((ch1 = GetObject(current, h1, HANDLE_TYPE_CHANNEL)) == NULL
        || (ch2 = GetObject(current, h2, HANDLE_TYPE_CHANNEL)) == NULL)
        return connectionErr;
    
    if (ch1 != ch2)
        return connectionErr;
        
    return 0;
}







/*
 * Inserts a handle into a memory segment so that it may be passed
 * inside a message. The handle can then be extracted with ExtractHandle().
 *
 * If the handle has already been marked as GRANT_ONCE it cannot be
 * injected into another message.
 *
 *
 * FIXME: The grant_once, we need a second flag and putmsg/getmsg to
 * check if it has previously been passed/granted.
 */

SYSCALL int PutHandle (int port_h, int h, bits32_t flags)
{
    struct Channel *channel;
    struct Process *current;
    struct Parcel *parcel;
    struct Handle *handle;
    int rq;
    
    
    current = GetCurrentProcess();
    
    if ((channel = GetObject(current, port_h, HANDLE_TYPE_CHANNEL)) == NULL)
        return paramErr;

    if ((handle = FindHandle(current, h)) == NULL)
        return paramErr;

    if (port_h == channel->handle[0])
        rq = 1;
    else
        rq = 0;
    
    if (channel->handle[rq] == -1)
        return connectionErr;

    if (handle->flags & HANDLEF_GRANTED_ONCE)
        return handleErr;
    
    DisablePreemption();
    
    parcel = LIST_HEAD (&free_parcel_list);
    LIST_REM_HEAD (&free_parcel_list, link);
    
    KASSERT (parcel != NULL);
    
    parcel->type = PARCEL_HANDLE;
    parcel->content.handle = h;
    
    LIST_ADD_TAIL (&channel->msg_list[rq], parcel, link);
    
    handle->parcel = parcel;
    handle->owner = NULL;
    
    if (flags & MSG_GRANT_ONCE)
        handle->flags |= HANDLEF_GRANTED_ONCE;
    
    if (!(flags & MSG_SILENT))
        DoRaiseEvent (channel->handle[rq]);
 
    return 0;
}




/*
 * Extracts a handle from a segment pointed to by addr.  Used for extracting
 * handles injected into message segments.
 */

SYSCALL int GetHandle (int port_h, int *rcv_h)
{
    struct Channel *channel;
    struct Process *current;
    struct Parcel *parcel;
    int h;
    int rq;
    
    
    current = GetCurrentProcess();
    
    if ((channel = GetObject(current, port_h, HANDLE_TYPE_CHANNEL)) == NULL)
        return paramErr;

    if (channel->handle[0] == -1 || channel->handle[1] == -1)
        return connectionErr;
    
    if (port_h == channel->handle[0])
        rq = 0;
    else
        rq = 1;
        
    parcel = LIST_HEAD (&channel->msg_list[rq]);
   
    if (parcel == NULL)
        return 0;
         
    if (parcel->type != PARCEL_HANDLE)
        return messageErr;
        
    h = parcel->content.handle;
    
    CopyOut (rcv_h, &h, sizeof (int));
    
    DisablePreemption();
    
    LIST_REM_HEAD (&channel->msg_list[rq], link);
    
    LIST_ADD_HEAD (&free_parcel_list, parcel, link);

    handle_table[h].parcel = NULL;
    handle_table[h].owner = current;

    return 1;
}



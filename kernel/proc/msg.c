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
 * Sends a multipart message comprising of a number memory segments through
 * a channel.  user_iov is an array of pointers to the segments to pass and
 * niov is the number of entries in this array.
 *
 * PutMsg() is a non blocking call and queues the segments on the receiving
 * end of the channel.  Further calls to PutMsg() will enqueue additional
 * segments on the receiving end.
 *
 * void *iov[1];
 * void *reply;
 * ssize_t reply_sz;
 *
 * while (1)
 * {
 *     // initialize message segment
 *     iov[0] = message_segment;
 *     PutMsg (handle, &iov[0], 1);
 *     WaitFor (handle);
 *     reply_sz = GetMsg (handle, &reply, PROT_READWRITE);
 *     // process reply
 * }
 */

SYSCALL int PutMsg (int h, void **user_iov, int niov)
{
    struct Channel *channel;
    struct Process *current;
    struct MemArea *matable[MAX_IOV_SEGMENTS];
    int rq;
    int t;
    
    if (CopyInIOVs (matable, (vm_addr *)user_iov, niov) != 0)
        return paramErr;
    
    current = GetCurrentProcess();

    if ((channel = GetObject(current, h, HANDLE_TYPE_CHANNEL)) == NULL)
        return paramErr;
    
    if (h == channel->handle[0])
        rq = 1;
    else
        rq = 0;
    
    if (channel->handle[rq] == -1)
        return connectionErr;

    DisablePreemption();

    for (t=0; t < niov; t++)
    {
        PmapRemoveRegion(&current->pmap, matable[t]);
        LIST_ADD_TAIL (&channel->msg_list[rq], matable[t], link);
        matable[t]->owner_process = NULL;
    }
    
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
 */

SYSCALL ssize_t GetMsg (int h, void **msg, bits32_t access)
{
    struct Channel *channel;
    struct Process *current;
    struct MemArea *ma;
    int rq;
    
    
    current = GetCurrentProcess();
    
    if ((channel = GetObject(current, h, HANDLE_TYPE_CHANNEL)) == NULL)
        return paramErr;
    
    if (h == channel->handle[0])
        rq = 0;
    else
        rq = 1;
        
    ma = LIST_HEAD (&channel->msg_list[rq]);
    
    if (ma == NULL && (channel->handle[0] == -1 || channel->handle[1] == -1))
        return connectionErr;
    else if (ma == NULL)
        return 0;
    
    if ((ma->flags & access & PROT_MASK) != (access & PROT_MASK))
        return messageErr;
    
    CopyOut (msg, &ma->base, sizeof (void *));
    
    DisablePreemption();
    
    LIST_REM_HEAD (&channel->msg_list[rq], link);
    
    if (LIST_HEAD (&channel->msg_list[rq]) == NULL)
        DoClearEvent (current, h);
    
    ma->owner_process = current;
    PmapEnterRegion (&current->pmap, ma, ma->base);
    return ma->ceiling - ma->base;
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
 * Any remaining message segments are freed by calling VirtualFree.  This
 * also places any embedded handles on the current process's close_handle_list
 * which gets harvested during KernelExit.
 *
 * AWOOGA: FIXME: CHECKME
 *
 * There are possible issues of sending a Channel handle to itself.
 * This can be done by injecting a Channel handle into a message
 * and then passing it to the Channel's second handle.
 *
 * We may want to come up with a solution to close/free all messages IF
 * one end of a channel is freed and the other end has a handle with NO owner.
 *
 * We have modified the code to free all segments and handles on BOTH pending
 * message lists.
 */

int DoCloseChannel (int h)
{
    struct Channel *ch;
    struct Process *current;
    struct MemArea *ma;
    struct Segment *seg;
    int u, v;
    
    
    current = GetCurrentProcess();
    
    if ((ch = GetObject (current, h, HANDLE_TYPE_CHANNEL)) == NULL)
        return paramErr;
            
    u = (ch->handle[0] == h) ? 0 : 1;    
    v = (ch->handle[1] == h) ? 1 : 0;
    
    DisablePreemption();
    
    while ((ma = LIST_HEAD (&ch->msg_list[u])) != NULL)
    {
        seg = SegmentFind (ma->physical_addr);
            
        if (seg->busy == TRUE)
            Sleep (&vm_rendez);
            
        ma->owner_process = current;
        
        LIST_REM_HEAD (&ch->msg_list[u], link);
        
        VirtualFree (ma->base);
    }
    

    while ((ma = LIST_HEAD (&ch->msg_list[v])) != NULL)
    {
        seg = SegmentFind (ma->physical_addr);
            
        if (seg->busy == TRUE)
            Sleep (&vm_rendez);
            
        ma->owner_process = current;
        
        LIST_REM_HEAD (&ch->msg_list[v], link);
        
        VirtualFree (ma->base);
    }
    
               
    ch->handle[u] = -1;
    
    if (ch->handle[v] == -1)
    {
        LIST_ADD_HEAD (&free_channel_list, ch, link);
        free_channel_cnt ++;
    }
    else
    {
        DoRaiseEvent (ch->handle[v]);
    }
    
    FreeHandle (h);
    return 0;
}




/*
 * Peforms a check to see if handle1 and handle2 both point to the same
 * channel.  Both handles must be owned by the current process.
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
 * AWOOGA: FIXME: CHECKME
 *
 * There are possible issues of sending a Channel handle to itself.
 * This can be done by injecting a Channel handle into a message
 * and then passing it to the Channel's second handle.
 *
 * May want a flag or separate system call to set the GRANT_ONCE flag
 * to handle.
 */

SYSCALL int InjectHandle (vm_addr addr, int h, bits32_t flags)
{
	struct Process *current;
	struct MemArea *ma;
	
	current = GetCurrentProcess();

	if (h < 0 || h >= max_handle || handle_table[h].type == HANDLE_TYPE_FREE
	    || handle_table[h].owner != current)
	{
	    return paramErr;
	}

	ma = MemAreaFind (addr);
		
	if (ma == NULL || ma->owner_process != current
	    || MEM_TYPE(ma->flags) != MEM_ALLOC)
	{
	    return memoryErr;
    }
    
	if (ma->msg_handle != -1)
	    return messageErr;
	    
    if (handle_table[h].flags & HANDLEF_GRANT_ONCE)
        return paramErr;	    
	
	DisablePreemption();
	
	if (handle_table[h].pending == 1)
		LIST_REM_ENTRY (&current->pending_handle_list, &handle_table[h], link);
    
    handle_table[h].flags = flags;
	handle_table[h].owner = NULL;
	ma->msg_handle = h;
	return 0;
}




/*
 * Extracts a handle from a segment pointed to by addr.  Used for extracting
 * handles injected into message segments.
 */

SYSCALL int ExtractHandle (vm_addr addr)
{
	struct Process *current;
	struct MemArea *ma;
	int h;
	
			
	current = GetCurrentProcess();
	
	ma = MemAreaFind (addr);
		
	if (ma == NULL || ma->owner_process != current
    	|| MEM_TYPE(ma->flags) != MEM_ALLOC)
	{
	    return memoryErr;
	}

	if (ma->msg_handle == -1)
	    return messageErr;
	
	DisablePreemption();

	handle_table[ma->msg_handle].owner = current;
	
	if (handle_table[ma->msg_handle].pending == 1)
		DoRaiseEvent (ma->msg_handle);
	
	h = ma->msg_handle;
	ma->msg_handle = -1;
	return h;
}



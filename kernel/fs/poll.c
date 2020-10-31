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

//#define KDEBUG

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <poll.h>
#include <kernel/proc.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/dbg.h>
#include <poll.h>

void WakeupPollsFromISR(struct VNode *vnode, short mask, short events);


/*
 * TODO:  Need to plug this into generic poll code ?????????
 * Need to check for terminated connections.
 * Need ot check closed end of pipes.
 *
 * TODO: Call into Handlers vfs_poll() to get state ?
 */
int VNodePoll(struct VNode *vnode, short mask, short *revents) {
  struct SuperBlock *sb;
  struct Msg *msg;
  
  sb = vnode->superblock;



// FIXME: S_ISPORT no longer exists, need to check filp->flags for F_SERVER
// Should also check if it is the root vnode.

// Hmmm, wonder if we should still go with second vnode for server.


  if (S_ISPORT(vnode->mode)) {
    msg = LIST_HEAD(&sb->msgport.pending_msg_list);
    
    if (msg != NULL) {
        *revents = POLLIN;
    } else {
        vnode->poll_events = 0;
    }

    
  } else if (S_ISREG(vnode->mode) || S_ISDIR(vnode->mode)) {
      
    if (mask & POLLPRI && (vnode->vnode_mounted_here != NULL 
                        || vnode->vnode_covered != NULL || vnode == sb->root)) {      
     *revents |= POLLPRI;
    }
    
    // Shouldn't these always be readable/writable ?
    
  } else if (S_ISCHR(vnode->mode)) {
  

    // TODO - poll() side, what do we check ?
    *revents = mask & vnode->poll_events;    
    
  } else if (S_ISIRQ(vnode->mode)) {
//    Info("Poll - INTERRUPT");
    
    *revents = *revents | (mask & vnode->poll_events);

    vnode->poll_events = 0;
  }
  
  return 0;
}



/*
 * TODO: FIXME:  Allocate Poll struct
 */
 
struct Poll *AllocPoll(void)
{
  struct Poll *poll;
  
  poll = LIST_HEAD(&poll_free_list);
  
  if (poll == NULL) {
    return NULL;
  }
  
  LIST_REM_HEAD(&poll_free_list, poll_link);
  return poll;
}

void FreePoll(struct Poll *poll)
{
  LIST_ADD_HEAD(&poll_free_list, poll, poll_link);
}



void PollTimeout(struct Timer *timer)
{
  timer->process->timeout_expired = TRUE;
  TaskWakeup (&timer->process->rendez);
}



int Poll (struct pollfd *pfds, nfds_t nfds, int timeout)
{
  struct Process *current;
  int nfds_matching;  
  LIST(Poll) poll_list;  
  struct Poll *poll;
  int sc;

  Info("Poll nfds = %d", nfds);
    
  current = GetCurrentProcess();
  current->poll_pending = FALSE;
  nfds_matching = 0;
  
  LIST_INIT(&poll_list);
    
  for (int t=0; t<nfds; t++) {
    struct pollfd pfd;
    struct Filp *filp;
    struct VNode *vnode;

    if (CopyIn (&pfd, &pfds[t], sizeof pfd) != 0) {
      sc = -EFAULT;
      goto exit;
    }
    
    
    // FIXME: Can we use -1 here to indicate skip this one?
    if (pfd.fd < 0) {
      sc = -EINVAL;
      goto exit;
    }
    
    filp = GetFilp(pfd.fd);
    
    if (filp == NULL) {
      sc = -EINVAL;
      goto exit;
    }

    vnode = filp->vnode;
    
    poll = AllocPoll();
    
    if (poll == NULL) {
      sc = -ENOMEM;
      goto exit;
    }
    
//    Info ("pfd[%d].fd: %d, addr: %08x, vnode: %08x events: %08x", t, pfd.fd, (vm_addr)poll, (vm_addr)vnode, pfd.events);
      
    LIST_ADD_TAIL (&poll_list, poll, poll_link);    
    poll->process = current;
    poll->vnode = vnode;
    poll->fd = pfd.fd;
    poll->events = pfd.events;
    poll->revents = 0;
    
    DisableInterrupts();
    LIST_ADD_TAIL(&vnode->poll_list, poll, vnode_link);
    VNodePoll(vnode, poll->events, &poll->revents);   // Attach in here.

    EnableInterrupts();
    
//    Info("Poll - 1st  - revent = %08x", poll->revents);

    if (poll->revents) {
      nfds_matching++;
    }
  }
  

//  Info ("Poll - first sweep matched %d  <<<<<<<<<<<<", nfds_matching);
  
  if (nfds_matching == 0) {
      // Non-matching (so add Poll to each vnode)
      // What if event comes in and we're blocking (on say alloc page due to copyin?)
      
    if (timeout != 0) {
      if (timeout != -1) {
//        Info("timout set");
        SetTimeout(timeout, PollTimeout, NULL);
      }

//      Info ("check if need to sleep");
      
      while(current->poll_pending == FALSE && current->timeout_expired == FALSE) {
        TaskSleep(&current->rendez);
      }

      if (current->eintr)
      {
//        KLog ("Wakened from EINTR");        
        current->eintr = FALSE;
      }

      if (timeout != -1) { 
//              Info("timout clr");     
        SetTimeout(0, NULL, NULL);
        current->timeout_expired = FALSE;
      }      
    }
  }
  
// Info ("Poll - Go through poll list");
  
  int idx = 0;
  nfds_matching = 0;
  
  
  // All polls should be valid.
  while ((poll = LIST_HEAD(&poll_list)) != NULL) {
    struct pollfd pfd;
    struct VNode *vnode;

    vnode = poll->vnode;
    VNodePoll(vnode, poll->events, &poll->revents);

    pfd.fd = poll->fd;
    pfd.events = poll->events;
    pfd.revents = poll->revents;

 //   Info ("pfd[%d].fd: %d, addr: %08x, vnode: %08x events: %08x", idx, pfd.fd, (vm_addr)poll, (vm_addr)vnode, pfd.events);
 //   Info("Poll - 2nd  - revent = %08x", poll->revents);

    
    
    if (pfd.revents != 0) {
      nfds_matching++;
    }

    LIST_REM_HEAD(&poll_list, poll_link);

    DisableInterrupts();
    LIST_REM_ENTRY(&vnode->poll_list, poll, vnode_link);        
    EnableInterrupts();
    
    CopyOut(&pfds[idx], &pfd, sizeof pfd);
    idx++;
    FreePoll(poll);
  }
  
  Info (".. Poll done - matching = %d", nfds_matching);
  return nfds_matching;
  
exit:
  Info (".. Poll error exit = %d", sc);

  while ((poll = LIST_HEAD(&poll_list)) != NULL)
  {
    struct VNode *vnode;

    vnode = poll->vnode;
    LIST_REM_ENTRY(&vnode->poll_list, poll, vnode_link);
    LIST_REM_HEAD(&poll_list, poll_link);
    FreePoll(poll);    
  }

  return sc;
}


int PollNotifyFromISR(struct InterruptAPI *api, uint32_t mask, uint32_t events)
{
  struct VNode *vnode;
  
  vnode = api->interrupt_vnode;
  WakeupPollsFromISR(vnode, mask, events);  
  return 0;
}


/*
 *
 */
int PollNotify (int fd, int ino, short mask, short events)
{
  struct Filp *filp;
  struct VNode *svnode;
  struct VNode *vnode;
  struct SuperBlock *sb;
  
  // Get server filp and then vnode
        
  filp = GetFilp(fd);

  if (filp == NULL) {
    return -EINVAL;
  }

  svnode = filp->vnode;

  sb = svnode->superblock;


  vnode = VNodeGet(sb, ino);
  
  if (vnode == NULL) {
    return -EINVAL;
  }

  
  WakeupPolls(vnode, mask, events);

  return 0;
}


/*
 *
 */
void WakeupPolls(struct VNode *vnode, short mask, short events)
{
  struct Process *proc;
  struct Poll *poll;

// TODO: Rewrite WakeupPolls (no longer have vnode->proces_interested bitmap);
  Info ("WakeupPolls(vnode = %08x)", (vm_addr)vnode);
  Info (".. , cov = %08x, mnthere = %08x", (vm_addr)vnode->vnode_covered, (vm_addr)vnode->vnode_mounted_here);
  vnode->poll_events = (vnode->poll_events & ~mask) | (mask & events);
  
  poll = LIST_HEAD(&vnode->poll_list);

  while (poll != NULL)
  {
      if (poll->events & vnode->poll_events)
      {      
          proc = poll->process;
          proc->poll_pending = TRUE;
          TaskWakeupAll(&proc->rendez);
          poll = LIST_NEXT(poll, vnode_link);
      }
  }  
}



/*
 *
 */
void WakeupPollsFromISR(struct VNode *vnode, short mask, short events)
{
  struct Process *proc;
  struct Poll *poll;

// TODO: Rewrite WakeupPolls (no longer have vnode->proces_interested bitmap);
//  Info ("WakeupPolls(vnode = %08x)", (vm_addr)vnode);

//  LedToggle();  
  
  vnode->poll_events |= POLLPRI;
  
  poll = LIST_HEAD(&vnode->poll_list);

  while (poll != NULL)
  {

//      if (poll->events & vnode->poll_events)
//      {      
          proc = poll->process;
          proc->poll_pending = TRUE;
          TaskWakeupFromISR(&proc->rendez);
//      }

    poll = LIST_NEXT(poll, vnode_link);
  }  
}





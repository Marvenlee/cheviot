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

// Prototypes
void WakeupPollsFromISR(struct VNode *vnode, short mask, short events);
int VNodePoll(struct VNode *vnode, short mask, short *revents);
struct Poll *alloc_poll(void);
void free_poll(struct Poll *poll);
void PollTimeout(struct Timer *timer);

/*
 *
 */
SYSCALL int sys_poll (struct pollfd *pfds, nfds_t nfds, int timeout)
{
  struct Process *current;
  int nfds_matching;  
  LIST(Poll) poll_list;  
  struct Poll *poll;
  int sc;

  current = get_current_process();
  current->poll_pending = false;
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
    
    filp = get_filp(pfd.fd);
    
    if (filp == NULL) {
      sc = -EINVAL;
      goto exit;
    }

    vnode = filp->vnode;
    
    poll = alloc_poll();
    
    if (poll == NULL) {
      sc = -ENOMEM;
      goto exit;
    }
    
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
    
    if (poll->revents) {
      nfds_matching++;
    }
  }
  
  if (nfds_matching == 0) {
      // Non-matching (so add Poll to each vnode)
      // What if event comes in and we're blocking (on say alloc page due to copyin?)
      
    if (timeout != 0) {
      if (timeout != -1) {
        SetTimeout(timeout, PollTimeout, NULL);
      }

      while(current->poll_pending == false && current->timeout_expired == false) {
        TaskSleep(&current->rendez);
      }

      if (current->eintr)
      {
        current->eintr = false;
      }

      if (timeout != -1) { 
        SetTimeout(0, NULL, NULL);
        current->timeout_expired = false;
      }      
    }
  }
  
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

    if (pfd.revents != 0) {
      nfds_matching++;
    }

    LIST_REM_HEAD(&poll_list, poll_link);

    DisableInterrupts();
    LIST_REM_ENTRY(&vnode->poll_list, poll, vnode_link);        
    EnableInterrupts();
    
    CopyOut(&pfds[idx], &pfd, sizeof pfd);
    idx++;
    free_poll(poll);
  }
  
  return nfds_matching;
  
exit:
  while ((poll = LIST_HEAD(&poll_list)) != NULL)
  {
    struct VNode *vnode;

    vnode = poll->vnode;
    LIST_REM_ENTRY(&vnode->poll_list, poll, vnode_link);
    LIST_REM_HEAD(&poll_list, poll_link);
    free_poll(poll);    
  }

  return sc;
}

/*
 *
 */
SYSCALL int sys_pollnotify (int fd, int ino, short mask, short events)
{
  struct Filp *filp;
  struct VNode *svnode;
  struct VNode *vnode;
  struct SuperBlock *sb;
  
  // Get server filp and then vnode
        
  filp = get_filp(fd);

  if (filp == NULL) {
    return -EINVAL;
  }

  svnode = filp->vnode;

  sb = svnode->superblock;


  vnode = vnode_get(sb, ino);
  
  if (vnode == NULL) {
    return -EINVAL;
  }
  
  wakeup_polls(vnode, mask, events);

  vnode_put(vnode);

  return 0;
}

/*
 *
 */
int PollNotifyFromISR(struct InterruptAPI *api, uint32_t mask, uint32_t events)
{
  struct VNode *vnode;

  // We don't need to lock it in an interrupt
  
  vnode = api->interrupt_vnode;
  WakeupPollsFromISR(vnode, mask, events);  
  return 0;
}

/*
 * FIXME: Call with vnode locked?
 */
void wakeup_polls(struct VNode *vnode, short mask, short events)
{
  struct Process *proc;
  struct Poll *poll;

  vnode->poll_events = (vnode->poll_events & ~mask) | (mask & events);
  
  poll = LIST_HEAD(&vnode->poll_list);

  while (poll != NULL)
  {
      if (poll->events & vnode->poll_events)
      {    
          proc = poll->process;
          proc->poll_pending = true;
          TaskWakeupAll(&proc->rendez);
      }

      poll = LIST_NEXT(poll, vnode_link);
  }
}

/*
 *
 */
void WakeupPollsFromISR(struct VNode *vnode, short mask, short events)
{
  struct Process *proc;
  struct Poll *poll;

  vnode->poll_events |= POLLPRI;
  
  poll = LIST_HEAD(&vnode->poll_list);

  while (poll != NULL)
  {
//      if (poll->events & vnode->poll_events)
//      {      
          proc = poll->process;
          proc->poll_pending = true;
          TaskWakeupFromISR(&proc->rendez);
//      }

    poll = LIST_NEXT(poll, vnode_link);
  }  
}

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
    *revents = *revents | (mask & vnode->poll_events);
    vnode->poll_events = 0;
  }
  
  return 0;
}

/*
 *
 */
struct Poll *alloc_poll(void)
{
  struct Poll *poll;
  
  poll = LIST_HEAD(&poll_free_list);
  
  if (poll == NULL) {
    return NULL;
  }
  
  LIST_REM_HEAD(&poll_free_list, poll_link);
  return poll;
}

/*
 *
 */
void free_poll(struct Poll *poll)
{
  LIST_ADD_HEAD(&poll_free_list, poll, poll_link);
}

/*
 *
 */
void PollTimeout(struct Timer *timer)
{
  timer->process->timeout_expired = true;
  TaskWakeup (&timer->process->rendez);
}


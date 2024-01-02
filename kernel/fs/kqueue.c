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

#define KDEBUG

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <poll.h>
#include <string.h>
#include <kernel/proc.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/dbg.h>
#include <kernel/kqueue.h>


/* @brief   Create a kqueue object in the current process
 *
 * @return  file descriptor of created kqueue or negative errno on error
 */
int sys_kqueue(void)
{
  struct Process *current;
  int fd;

  Info("sys_kqueue");
  
  current = get_current_process();
  fd = alloc_fd_kqueue(current);

  return fd;
}


/* @brief   Register interest and wait for kernel events
 *
 * @param   fd, file descriptor of kqueue
 * @param   changelist, array of kevent structures to change the settings of 
 * @param   nchanges, number of kevents in changelist array
 * @param   eventlist, array to receive new kevents into
 * @param   nevents, size of eventlist array
 * @param   timeout, maximum time to wait to receive an event
 * @return  number of events copied into eventlist buffer or negative errno on error
 */
int sys_kevent(int fd, const struct kevent *changelist, int nchanges,
           struct	kevent *eventlist, int nevents, const struct timespec *_timeout)
{
  int nevents_returned;
  struct kevent ev;
  struct KQueue *kqueue;
  struct KNote *knote;
  struct Process *current;
  struct timespec timeout;
  bool timeout_valid = false;
  bool timedout = false;
  int sc;
    
  current = get_current_process();

  if (_timeout != NULL) {
    CopyIn(&timeout, _timeout, sizeof timeout);
    timeout_valid = true;
  }
  
  kqueue = get_kqueue(current, fd);
  
  while (kqueue->busy == true) {
    TaskSleep(&kqueue->busy_rendez);
  }

  kqueue->busy = true;
  
  if (nchanges > 0 && changelist != NULL) {
    for (int t = 0; t < nchanges; t++) {      
      CopyIn(&ev, &changelist[t], sizeof ev);
      
      if (ev.filter < 0 || ev.filter >= EVFILT_SYSCOUNT) {
        sc = -EINVAL;
        goto exit;      
      }
      
      knote = get_knote(kqueue, &ev);

      if (ev.flags & EV_ADD) {
        if (knote == NULL) {
        
          // FIXME: Currently automatically enabling this event on EV_ADD.
          // TODO: Handle enabling/disabling separately with EV_ENABLE, EV_DISABLE
          
          // Allow EV_ADD and EV_DISABLE (filters events but won't return them).
          // Allow EV_ADD on its own to enable it.
          
          // EV_DELETE and any other flags are ignored.
          
          // FIXME: IMPORTANT : To avoid a race condition which would cause us to wait
          // for an event that has already happened we need to check the state of objects
          // that the newly added events point to in order to add events to the pending
          // queue.
          //
          // e.g.
          // kq = kqueue()
          // fd = mount("/path/to/mount", 0, mode)/
          //
          // A message could immediately arrive here, but wouldn't be picked up
          // by current kqueue code, causing kevent to block until a timeout or
          // other event occurs.
          // 
          // EV_SET(&ev, portid, EVFILT_MSGPORT, EV_ADD | EV_ENABLE, 0, 0, 0); 
          // kevent(kq, &ev, 1,  &ret_ev, 1, NULL);
          //
          // Handle above in EV_ENABLE
          
          knote = alloc_knote(kqueue, &ev);
    
          if (knote == NULL) {
            sc = -ENOMEM;
            goto exit;
          }
          
          
          
        } else {
          sc = -EEXIST;
          goto exit;
        }        
      } else if (ev.flags & EV_DELETE) {
        if (knote != NULL) {
          free_knote(kqueue, knote);        
        } else {
          sc = -EINVAL;
          goto exit;
        }
      } else {
        if (knote != NULL) {
          // Modify existing knote enable/disable/oneshot flags
          // update fflags   
        } else {
          sc = -EINVAL;
          goto exit;
        }    
      }
    }
  }
  
  nevents_returned = 0;

  if (nevents > 0 && eventlist != NULL) {
    while (LIST_HEAD(&kqueue->pending_list) == NULL) {
      if (timeout_valid == true) {
        if (TaskTimedSleep(&kqueue->event_rendez, &timeout) != 0) {
          timedout = true;
          break;         
        }        
      } else {
        TaskSleep(&kqueue->event_rendez);
      }      
    }
    
    if (timedout == false) {
      while (nevents_returned < nevents) {
        knote = LIST_HEAD(&kqueue->pending_list);
              
        if (knote == NULL) {
          break;
        }

        memset(&ev, 0, sizeof ev);
        ev.ident = knote->ident;  
        ev.filter = knote->filter;          
        ev.flags  = knote->flags;
        ev.fflags = knote->fflags;
        ev.udata = knote->udata;
        
        CopyOut(&eventlist[nevents_returned], &ev, sizeof ev);
        nevents_returned++;
        

        LIST_REM_HEAD(&kqueue->pending_list, pending_link);
        knote->pending = false;
        knote->on_pending_list = false;
                
        if (knote->flags & EV_ONESHOT) {
          free_knote(kqueue, knote);
        }    
      }
    }
  }

  kqueue->busy = false;
  TaskWakeup(&kqueue->busy_rendez);  
  
  if (timedout) {
    return -ETIMEDOUT;
  }
  
  return nevents_returned;

exit:
  // TODO: Any kevent cleanup
  return sc;
}


/* @brief   Close a kqueue file descriptor
 *
 * @param   proc,
 * @param   fd,
 * @return  0 on success, negative errno on error
 */
int close_kqueue(struct Process *proc, int fd)
{
  // Delete all Notes on list
  
  free_fd_kqueue(proc, fd);
  return 0;
}


/* @brief   Add an event note to a kqueue's pending list
 *
 * @param   knote_list, list of knotes attached to an object
 * @param   hint, a hint as to why the knote was added
 * @return  0 on success, negative errno on error
 */
int knote(knote_list_t *knote_list, int hint)
{
  struct KNote *knote;
  struct KQueue *kqueue;

  knote = LIST_HEAD(knote_list);
  
  while(knote != NULL) {
    knote->pending = true;
    knote->hint = hint;

    if ((knote->flags & EV_ENABLE) && knote->on_pending_list == false) {        
      kqueue = knote->kqueue;
      
      LIST_ADD_TAIL(&kqueue->pending_list, knote, pending_link);
      knote->on_pending_list = true;
      
      TaskWakeup(&kqueue->event_rendez);
    }
    
    knote = LIST_NEXT(knote, object_link);
  }
    
  return 0;
}


/*
 *
 */
struct KNote *get_knote(struct KQueue *kq, struct kevent *ev)
{
  uint16_t hash;
  struct KNote *knote;
  
  hash = knote_calc_hash(kq, ev->ident, ev->filter);
  knote = LIST_HEAD(&knote_hash[hash]);
  
  while (knote != NULL) {
    if (knote->kqueue == kq && knote->ident == ev->ident && knote->filter == ev->filter) {
      return knote;
    }

    knote = LIST_NEXT(knote, hash_link);
  }
  
  return NULL;  
}


/*
 *
 */
struct KQueue *get_kqueue(struct Process *proc, int fd)
{
  struct Filp *filp;
  
  filp = get_filp(proc, fd);
  
  if (filp == NULL) {
    return NULL;
  }
  
  if (filp->type != FILP_TYPE_KQUEUE) {
    return NULL;
  }

  return filp->u.kqueue;
}


/* @brief   Allocate a kqueue and associated file descriptor
 *
 * Allocates a handle structure.  Checks to see that free_handle_cnt is
 * non-zero should already have been performed prior to calling alloc_fd().
 */
int alloc_fd_kqueue(struct Process *proc)
{
  int fd;
  struct KQueue *kqueue;
  
  fd = alloc_fd_filp(proc);
  
  if (fd < 0) {
    return -EMFILE;
  }
  
  kqueue = alloc_kqueue();
  
  if (kqueue == NULL) {
    free_fd_filp(proc, fd);
    return -EMFILE;
  }
  
  kqueue->reference_cnt=1;
  set_fd(proc, fd, FILP_TYPE_KQUEUE, 0, kqueue);
  
  return fd;
}


/* @brief   Free a kqueue and associated file descriptor
 *
 * Marks the file descriptor as free, decrements reference counts
 * and if the kqueue is no longer referenced it is freed.
 */
int free_fd_kqueue(struct Process *proc, int fd)
{
  struct KQueue *kqueue;
  
  kqueue = get_kqueue(proc, fd);

  if (kqueue == NULL) {
    return -EINVAL;
  }

  free_fd_filp(proc, fd);
  free_kqueue(kqueue);
  return 0;
}


/*
 *
 */
struct KQueue *alloc_kqueue(void)
{
  struct KQueue *kqueue;

  kqueue = LIST_HEAD(&kqueue_free_list);

  if (kqueue == NULL) {
    return NULL;
  }
  
  LIST_REM_HEAD(&kqueue_free_list, free_link);

  kqueue->busy = false;
  kqueue->reference_cnt = 0;

  InitRendez(&kqueue->busy_rendez);
  InitRendez(&kqueue->event_rendez);
  
  LIST_INIT(&kqueue->knote_list);
  LIST_INIT(&kqueue->pending_list);
  
  return kqueue;
}


/*
 *
 */
void free_kqueue(struct KQueue *kqueue)
{
  kqueue->reference_cnt--;
  
  if (kqueue->reference_cnt == 0) {
    // TODO: We need to detach any knotes from the kqueue before freeing it.
    LIST_ADD_HEAD(&kqueue_free_list, kqueue, free_link);
  }
}


/*
 *
 */
struct KNote *alloc_knote(struct KQueue *kqueue, struct kevent *ev)
{
  int sc = 0;
  int hash;
  struct KNote *knote;
  struct SuperBlock *sb;
  struct VNode *vnode;
  struct ISRHandler *isrhandler;
  struct Process *current;
  
  current = get_current_process();
  
  if ((knote = LIST_HEAD(&knote_free_list)) == NULL) {
    return NULL;
  }
  
  LIST_REM_HEAD(&knote_free_list, link);  
  LIST_ADD_TAIL(&kqueue->knote_list, knote, kqueue_link);

  hash = knote_calc_hash(kqueue, knote->ident, knote->filter);
  LIST_ADD_TAIL(&knote_hash[hash], knote, hash_link);
  
  knote->kqueue = kqueue;
  knote->ident = ev->ident;  
  knote->filter = ev->filter;          
  knote->flags = ev->flags;
  knote->fflags = ev->fflags;
  knote->data = NULL;
  knote->udata = ev->udata;  
  knote->pending = false;  
  knote->on_pending_list = false;
  knote->object = NULL;
    
  switch (knote->filter) {
    case EVFILT_READ:
    case EVFILT_WRITE:
    // TODO: Add EVFILT_FS for mount/unmount events    
    case EVFILT_VNODE:
      vnode = get_fd_vnode(current, knote->ident);
      
      if (vnode) {
        knote->object = vnode;
        LIST_ADD_TAIL(&vnode->knote_list, knote, object_link);
      } else {
        sc = -EINVAL;
      }
      break;
      
    case EVFILT_AIO:
      sc = -ENOSYS;
      break;

    case EVFILT_PROC:
      sc = -ENOSYS;
      break;
      
    case EVFILT_SIGNAL:    
      sc = -ENOSYS;
      break;
      
    case EVFILT_TIMER:
      sc = -ENOSYS;
      break;
      
    case EVFILT_NETDEV:
      sc = -ENOSYS;
      break;
      
    case EVFILT_USER:
      sc = -ENOSYS;
      break;
      
    case EVFILT_IRQ:
      isrhandler = get_isrhandler(current, knote->ident);
      
      if (isrhandler) {
        knote->object = isrhandler;
        LIST_ADD_TAIL(&isrhandler->knote_list, knote, object_link);      
      } else {
        sc = -ENOSYS;
      }      
      break;
    case EVFILT_MSGPORT:
      sb = get_superblock(current, knote->ident);
    
      if (sb) {
        knote->object = sb;
        LIST_ADD_TAIL(&sb->msgport.knote_list, knote, object_link);
      } else {
        sc = -EINVAL;
      }
      
      break;
      
    default:
      sc = -ENOSYS;

      // kernel panic  
  }

  if (sc != 0) {
    free_knote(kqueue, knote);
    return NULL;
  }

  return knote;
}


/* @brief   Free a knote
 *
 * @param   kqueue,
 * @param   knote,
 *
 * Removes the knote from the kqueue and the object's knote list 
 */
void free_knote(struct KQueue *kqueue, struct KNote *knote)
{
  int hash;
  struct SuperBlock *sb;
  struct VNode *vnode;
  struct ISRHandler *isrhandler;
  struct Process *current;
  
  current = get_current_process();
  
  switch (knote->filter) {
    case EVFILT_READ:
    case EVFILT_WRITE:
    // TODO: ADd EVFILT_FS for mount/unmount events
    case EVFILT_VNODE:
      vnode = get_fd_vnode(current, knote->ident);
      
      if (vnode) {
        knote->object = NULL;
        LIST_REM_ENTRY(&vnode->knote_list, knote, object_link);
      }
      break;
      
    case EVFILT_AIO:
      break;

    case EVFILT_PROC:
      break;
      
    case EVFILT_SIGNAL:    
      break;
      
    case EVFILT_TIMER:
      break;
      
    case EVFILT_NETDEV:
      break;
      
    case EVFILT_USER:
      break;
      
    case EVFILT_IRQ:
      isrhandler = get_isrhandler(current, knote->ident);
      
      if (isrhandler) {
        knote->object = NULL;
        LIST_REM_ENTRY(&isrhandler->knote_list, knote, object_link);      
      }      
      break;
      
    case EVFILT_MSGPORT:
      sb = get_superblock(current, knote->ident);
      
      if (sb) {
        knote->object = NULL;
        LIST_REM_ENTRY(&sb->msgport.knote_list, knote, object_link);
      }
      break;
      
    default:
      break;
  }

  if (knote->on_pending_list == true) {
    LIST_REM_ENTRY(&kqueue->pending_list, knote, pending_link);
    knote->on_pending_list = false;
  }
  
  knote->pending = false;  
  LIST_REM_ENTRY(&kqueue->knote_list, knote, kqueue_link);  

  hash = knote_calc_hash(kqueue, knote->ident, knote->filter);
  LIST_REM_ENTRY(&knote_hash[hash], knote, hash_link);

  LIST_ADD_HEAD(&knote_free_list, knote, link);
}


/* @brief   Enable an existing knote
 *
 */
void enable_knote(struct KQueue *kqueue, struct KNote *knote)
{
  // TODO: Mark knote as enabled.
  // TODO: Check if we need to raise any pending events,
  // e.g. if file has data in buffer for reading or
  // if message is already on message port.
  
}


/* @brief   Disable an existing knote
 *
 */
void disable_knote(struct KQueue *kqueue, struct KNote *knote)
{
  // TODO: This should be called prior to deleting a knote
}


/* @brief   Calculate a hash value for looking up a knote
 */
int knote_calc_hash(struct KQueue *kq, int ident, int filter)
{
  return ((ident << 8) | filter) % KNOTE_HASH_SZ;
}




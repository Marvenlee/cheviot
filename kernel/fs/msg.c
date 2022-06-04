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
 * Message passing system calls that the VFS uses to communicate with servers that
 * implement filesystem handlers, block and character device drivers.
 *
 * Filesystem requests are converted to multiport IOV messages.  The server
 * typically uses Poll() to wait for a message to arrive. The server then uses
 * calls ReceiveMsg to receive the first part of a message.  The remainder of
 * the message can be read or modified using ReadMsg, WriteMsg and SeekMsg. The
 * server indicates it is finished with the message by calling ReplyMsg.
 */

// #define KDEBUG

#include <kernel/arm/boot.h>
#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>
#include <string.h>
#include <poll.h>


/*
 *
 */
int InitMsgPort(struct MsgPort *msgport, struct VNode *vnode) {

  LIST_INIT(&msgport->pending_msg_list);
  msgport->server_vnode = vnode;

  Info ("InitMsgPort port:%08x, vnode:%08x", msgport, vnode);

  return 0;
}

/*
 * Queue message but don't wait
 */
int KPutMsg(struct MsgPort *port, struct VNode *vnode, struct Msg *msg)
{
  return 0;
}

/*
 * TODO:  Need timeout and abort mechanisms into IPC in case server doesn't respond or terminates.
 * Also timeout works for when a server tries to create a second mount in its own mount path.
 */
int KSendMsg(struct MsgPort *port, struct VNode *vnode, struct Msg *msg) {
  struct Process *current;

  current = GetCurrentProcess();
  
  Info ("KSendMsg...");
  KASSERT (port != NULL);
  KASSERT (vnode != NULL);
  KASSERT (msg != NULL);
  KASSERT (port->server_vnode != NULL);
  KASSERT (current->msg == NULL);

  Info ("msg = %08x", msg);
  Info ("port = %08x", port);
  Info ("vnode = %08x", vnode);
  Info ("port->server_vnode = %08x", port->server_vnode);
  

  current->msg = msg;  
  msg->offset = 0;
  msg->state = MSG_STATE_SEND;
  msg->pid = current->pid;
  msg->port = port;
  InitRendez(&msg->rendez);  
  LIST_ADD_TAIL(&port->pending_msg_list, msg, link);

  Info ("port->server_vnode = %08x", port->server_vnode);
  
  WakeupPolls(port->server_vnode, POLLIN, POLLIN);

  Info ("Wakeup polls sendmsg ok");

  while (msg->state != MSG_STATE_REPLIED) {
    TaskSleep(&msg->rendez);
  }

  current->msg = NULL;  
  return msg->reply_status;
}

/*
 *
 */
SYSCALL int SysReceiveMsg(int server_fd, int *pid, void *buf, size_t buf_sz) {
  struct Filp *filp;
  struct VNode *svnode;
  struct SuperBlock *sb;
  struct Msg *msg;
  int nbytes_read;
  int nbytes_to_xfer;
  int remaining;
  int i;
  
  Info ("SysReceiveMsg, fd=%d", server_fd);
  
  filp = GetFilp(server_fd);

  if (filp == NULL) {
    Info("ReceiveMsg filp==NULL");
    return -EINVAL;
  }

  svnode = filp->vnode;
  sb = svnode->superblock;

  Info("svnode->superblock %08x", sb);
  
  msg = LIST_HEAD(&sb->msgport.pending_msg_list);

  // FIXME: Should ReceiveMsg be blocking instead?

  if (msg == NULL) {
    Info("ReceiveMsg no msg, returning 0");
    return 0;    
  }

  LIST_REM_HEAD(&sb->msgport.pending_msg_list, link);
  msg->state = MSG_STATE_RECEIVED;

  CopyOut(pid, &msg->pid, sizeof *pid);

  i = 0;
  nbytes_read = 0;
    
  while (msg->offset < buf_sz) {
    remaining = buf_sz - msg->offset;

    nbytes_to_xfer =
        (msg->iov[i].size < remaining) ? msg->iov[i].size : remaining;

    CopyOut(buf + nbytes_read, msg->iov[i].addr, nbytes_to_xfer);
    nbytes_read += nbytes_to_xfer;
    msg->offset += nbytes_to_xfer;
    i++;
  }

  Info("ReceiveMsg nbytes:%d, pid = %d", nbytes_read, msg->pid);

  return nbytes_read;
}

/*
 *
 */
SYSCALL int SysReplyMsg(int server_fd, int pid, int status) {
  struct Filp *filp;
  struct VNode *svnode;
  struct Msg *msg;
  struct Process *proc;

  Info("StsReplyMsg fd:%d, pid:%d", server_fd, pid);

  filp = GetFilp(server_fd);

  if (filp == NULL) {
    Info ("ReplyMsg, invalid server_fd");
    return -EINVAL;
  }

  svnode = filp->vnode;

  proc = GetProcess(pid);
  
  if (proc == NULL) {
    Info("ReplyMsg: proc==NULL");
    return -EINVAL;
  }

  msg = proc->msg;
  
  if (msg == NULL) {
    Info("ReplyMsg: msg==NULL");
    return -EINVAL;
  }
  
  if (msg->port->server_vnode != svnode) {
    Info("ReplyMsg: server_vnode != svnode");
      return -EINVAL;
  }
  
  if (msg->state != MSG_STATE_RECEIVED) {
      Info("ReplyMsg: state != MSG_STATE_RECEIVED");
      return -EINVAL;
  }
    
  msg->reply_status = status;
  msg->state = MSG_STATE_REPLIED;
  
  TaskWakeup(&msg->rendez);
  
  // For Async messages, need to handle either single (-1) pid OR give each buf a PID.
  return 0;
}

/*
 *
 */
SYSCALL int SysReadMsg(int server_fd, int pid, void *buf, size_t buf_sz) {
  struct Filp *filp;
  struct VNode *svnode;
  int nbytes_to_read;
  int nbytes_read;
  int base_offset;
  int remaining;
  int iov_remaining;
  int i;
  struct Msg *msg;
  struct Process *proc;
  
  filp = GetFilp(server_fd);

  if (filp == NULL) {
    return -EINVAL;
  }

  svnode = filp->vnode;

  proc = GetProcess(pid);
  
  if (proc == NULL) {
    return -EINVAL;
  }
  
  msg = proc->msg;

  if (msg == NULL) {
    return -EINVAL;
  }
  
  if (msg->port->server_vnode != svnode) {
      return -EINVAL;
  }
  
  if (msg->state != MSG_STATE_RECEIVED) {
      return -EINVAL;
  }
    
  nbytes_read = 0;
  base_offset = 0;

  for (i = 0; i < msg->iov_cnt; i++) {
    if (msg->offset >= base_offset &&
        msg->offset < base_offset + msg->iov[i].size) {
      iov_remaining = base_offset + msg->iov[i].size - msg->offset;
      break;
    }

    base_offset += msg->iov[i].size;
  }

  while (i < msg->iov_cnt && nbytes_read < buf_sz) {
    remaining = buf_sz - nbytes_read;
    nbytes_to_read = (remaining < iov_remaining) ? remaining : iov_remaining;

    CopyOut(buf + nbytes_read,
            msg->iov[i].addr + msg->iov[i].size - iov_remaining,
            nbytes_to_read);
    nbytes_read += nbytes_to_read;
    msg->offset += nbytes_to_read;
    i++;
    iov_remaining = msg->iov[i].size;
  }

  return nbytes_read;
}

/*
 *
 */
SYSCALL int SysWriteMsg(int server_fd, int pid, void *buf, size_t buf_sz) {
  struct Filp *filp;
  struct VNode *svnode;
  int nbytes_to_write;
  int nbytes_written;
  int base_offset;
  int remaining;
  int iov_remaining;
  int i;
  struct Msg *msg;
  struct Process *proc;
        int sc;
  
  filp = GetFilp(server_fd);

  if (filp == NULL) {
    return -EINVAL;
  }

  svnode = filp->vnode;
  
  proc = GetProcess(pid);
  
  if (proc == NULL) {
    return -EINVAL;
  }
  
  msg = proc->msg;
  
  if (msg == NULL) {
    return -EINVAL;
  }

  if (msg->port->server_vnode != svnode) {
      return -EINVAL;
  }
  
  if (msg->state != MSG_STATE_RECEIVED) {
      return -EINVAL;
  }

  nbytes_written = 0;
  base_offset = 0;

  for (i = 0; i < msg->iov_cnt; i++) {
    if (msg->offset >= base_offset &&
        msg->offset < base_offset + msg->iov[i].size) {
      iov_remaining = base_offset + msg->iov[i].size - msg->offset;
      break;
    }

    base_offset += msg->iov[i].size;
  }

  while (i < msg->iov_cnt && nbytes_written < buf_sz) {
    remaining = buf_sz - nbytes_written;
    nbytes_to_write = (remaining < iov_remaining) ? remaining : iov_remaining;

    sc = CopyIn(msg->iov[i].addr + msg->iov[i].size - iov_remaining,
           buf + nbytes_written, nbytes_to_write);
     
    if (sc != 0) {
        break;
    }
    
    nbytes_written += nbytes_to_write;

    msg->offset += nbytes_to_write;

    i++;
    iov_remaining = msg->iov[i].size;
  }

//  Info ("WriteMsg nbytes_written:%d", nbytes_written);
  return nbytes_written;
}



SYSCALL int SysSeekMsg(int server_fd, int pid, off_t offset) {
  struct Filp *filp;
  struct VNode *svnode;
  struct Process *proc;
  struct Msg *msg;
  
  filp = GetFilp(server_fd);

  if (filp == NULL) {
    return -EINVAL;
  }

  svnode = filp->vnode;
  proc = GetProcess(pid);
  
  if (proc == NULL) {
    return -EINVAL;
  }
  
  msg = proc->msg;
  
  if (msg == NULL) {
    return -EINVAL;
  }

  if (msg->port->server_vnode != svnode) {
      return -EINVAL;
  }
  
  if (msg->state != MSG_STATE_RECEIVED) {
      return -EINVAL;
  }

  msg->offset = offset;
  return 0;
}


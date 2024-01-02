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
 * Filesystem requests are converted to multi-part IOV messages.  The server
 * typically uses kqueue's kevent() to wait for a message to arrive. The server
 * then uses ReceiveMsg to receive the header of a message.  The remainder of
 * the message can be read or modified using ReadMsg and WriteMsg. The
 * server indicates it is finished with the message by calling ReplyMsg.
 */

//#define KDEBUG

#include <kernel/dbg.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>
#include <string.h>
#include <kernel/kqueue.h>


/* @brief   Send a knote event to a vnode in the kernel
 *
 * @param   fd, file descriptor of mount on which the file exists
 * @param   ino_nr, inode number of the file
 * @param   hint, hint of why this is being notified. *
 * @return  0 on success, negative errno on error
 *
 * Perhaps change it to sys_setvnodeattrs(fd, ino_nr, flags);
 */
int sys_knotei(int fd, int ino_nr, long hint)
{
  struct SuperBlock *sb;
  struct VNode *vnode;
  struct Process *current;
  
  current = get_current_process();  
  sb = get_superblock(current, fd);
  
  if (sb == NULL) {
    return -EINVAL;
  }
  
  vnode = vnode_get(sb, ino_nr);
  
  if (vnode == NULL) {
    return -EINVAL;
  }
    
  knote(&vnode->knote_list, hint);  
  vnode_put(vnode);
  return 0;
}


/* @brief   Get a message from a mount's message port
 *
 * @param   fd, file descriptor of mount created by sys_mount()
 * @param   msgidp, user address to store the message's msgid
 * @param   addr, address of buffer to read the initial part of message into
 * @param   buf_sz, size of buffer to read into
 * @return  number of bytes read or negative errno on error.
 *
 * This is a non-blocking function.  Use in conjunction with kevent() in order
 * to wait for messages to arrive.
 *
 * TODO: Revert msgid to sender's process ID + thread ID, change type to endpoint_t.
 * 
 */
int sys_getmsg(int fd, msgid_t *msgid, void *addr, size_t buf_sz)
{
  struct SuperBlock *sb;  
  struct MsgPort *msgport;
  struct Msg *msg;
  off_t offset;
  int nbytes_read;
  int nbytes_to_xfer;
  int remaining;
  int i;
  struct Process *current;

  Info("sys_getmsg (addr:%08x, sz:%d", addr, buf_sz);

  current = get_current_process();  
  sb = get_superblock(current, fd);
  
  if (sb == NULL) {
    return -EINVAL;
  }
  
  msgport = &sb->msgport;
  
  msg = kgetmsg(msgport);

  if (msg == NULL) {
    return 0;
  }  
  
  CopyOut (msgid, &msg->msgid, sizeof *msgid);

  i = 0;
  nbytes_read = 0;
  offset = 0;
  
  while (i < msg->siov_cnt && nbytes_read < buf_sz) {
    remaining = buf_sz - offset;
    nbytes_to_xfer = (msg->siov[i].size < remaining) ? msg->siov[i].size : remaining;

    CopyOut(addr + nbytes_read, msg->siov[i].addr, nbytes_to_xfer);
    nbytes_read += nbytes_to_xfer;
    offset += nbytes_to_xfer;
    i++;
  }

  return nbytes_read;
}


/* @brief   Reply to a message
 *
 * @param   fd, file descriptor of mounted file system created with sys_mount()
 * @param   msgid, unique message identifier returned by sys_receivemsg()
 * @param   status, error status to return to caller (0 on success or negative errno)
 * @param   addr, address of buffer to write from
 * @param   buf_sz, size of buffer to write from
 * @return  0 on success, negative errno on error 
 */
int sys_replymsg(int fd, msgid_t *_msgid, int status, void *addr, size_t buf_sz)
{
  msgid_t msgid;
  struct SuperBlock *sb;
  struct MsgPort *msgport;
  struct Msg *msg;
  int nbytes_to_write;
  int nbytes_written;
  int remaining;
  int iov_remaining;
  int i;
  int sc;
  off_t offset = 0;
  struct Process *current;

  Info("sys_replymsg (msgid %d, addr:%08x, sz:%d", msgid, addr, buf_sz);

  if (CopyIn(&msgid, _msgid, sizeof msgid) != 0) {
    return -EFAULT;
  }
    
  current = get_current_process();  
  sb = get_superblock(current, fd);

  if (sb == NULL) {
    return -EINVAL;
  }
  
  msgport = &sb->msgport;

  if ((msg = msgid_to_msg(msgport, msgid)) == NULL) {
    return -EINVAL;
  }

  msg->reply_status = status;

  iov_remaining = msg->riov[0].size;

  while (i < msg->riov_cnt && nbytes_written < buf_sz) {
    remaining = buf_sz - nbytes_written;
    nbytes_to_write = (msg->riov[i].size < remaining) ? msg->riov[i].size : remaining;

    sc = CopyIn(msg->riov[i].addr + msg->riov[i].size - iov_remaining,
           addr + nbytes_written, nbytes_to_write);
     
    if (sc != 0) {
        break;
    }
    
    nbytes_written += nbytes_to_write;

    offset += nbytes_to_write;

    i++;
    iov_remaining = msg->riov[i].size;
  }

  kreplymsg(msg);

  return 0;
}


/* @brief   Read from a message
 *
 * @param   fd, file descriptor of mounted file system created with sys_mount()
 * @param   msgid, unique message identifier returned by sys_receivemsg()
 * @param   addr, address of buffer to read into
 * @param   buf_sz, size of buffer to read into
 * @param   offset, offset within the message to read
 * @return  number of bytes read on success, negative errno on error 
 */
int sys_readmsg(int fd, msgid_t *_msgid, void *addr, size_t buf_sz, off_t offset)
{
  msgid_t msgid;
  struct SuperBlock *sb;
  struct MsgPort *msgport;
  struct Msg *msg;
  int nbytes_to_read;
  int nbytes_read;
  int remaining;
  int iov_remaining;
  int i;
  struct Process *current;

  if (CopyIn(&msgid, _msgid, sizeof msgid) != 0) {
    return -EFAULT;
  }
    
  current = get_current_process();  
  sb = get_superblock(current, fd);
  
  if (sb == NULL) {
    return -EINVAL;
  }
  
  msgport = &sb->msgport;

  if ((msg = msgid_to_msg(msgport, msgid)) == NULL) {
    return -EINVAL;
  }
  
  if (seekiov(msg->siov_cnt, msg->siov, offset, &i, &iov_remaining) != 0) {
    return -EINVAL;
  }

  nbytes_read = 0;

  while (i < msg->siov_cnt && nbytes_read < buf_sz) {
    remaining = buf_sz - nbytes_read;
    nbytes_to_read = (remaining < iov_remaining) ? remaining : iov_remaining;

    CopyOut(addr + nbytes_read,
            msg->siov[i].addr + msg->siov[i].size - iov_remaining,
            nbytes_to_read);

    nbytes_read += nbytes_to_read;
    offset += nbytes_to_read;
    i++;
    iov_remaining = msg->siov[i].size;
  }
  
  return nbytes_read;
}


/* @brief   Write to a message
 *
 * @param   fd, file descriptor of mounted file system created with sys_mount()
 * @param   msgid, unique message identifier returned by sys_receivemsg()
 * @param   addr, address of buffer to write from
 * @param   buf_sz, size of buffer to write from
 * @param   offset, offset within the message to write
 * @return  number of bytes written on success, negative errno on error 
 */
int sys_writemsg(int fd, msgid_t *_msgid, void *addr, size_t buf_sz, off_t offset)
{
  msgid_t msgid;
  struct SuperBlock *sb;
  struct MsgPort *msgport;
  struct Msg *msg;
  int nbytes_to_write;
  int nbytes_written;
  int remaining;
  int iov_remaining;
  int i;
  int sc;
  struct Process *current;

  if (CopyIn(&msgid, _msgid, sizeof msgid) != 0) {
    return -EFAULT;
  }
  
  current = get_current_process();  
  sb = get_superblock(current, fd);
  
  if (sb == NULL) {
    return -EINVAL;
  }
  
  msgport = &sb->msgport;
  
  if ((msg = msgid_to_msg(msgport, msgid)) == NULL) {
    return -EINVAL;
  }
  
  if (msg->port != msgport) {
    return -EINVAL;
  }
  
  if (seekiov(msg->riov_cnt, msg->riov, offset, &i, &iov_remaining) != 0) {
    return -EINVAL;
  }

  nbytes_written = 0;
  
  while (i < msg->riov_cnt && nbytes_written < buf_sz) {
    remaining = buf_sz - nbytes_written;
    nbytes_to_write = (remaining < iov_remaining) ? remaining : iov_remaining;

    sc = CopyIn(msg->riov[i].addr + msg->riov[i].size - iov_remaining,
           addr + nbytes_written, nbytes_to_write);
     
    if (sc != 0) {
        break;
    }
    
    nbytes_written += nbytes_to_write;

    offset += nbytes_to_write;

    i++;
    iov_remaining = msg->riov[i].size;
  }

  return nbytes_written;
}


/* @brief   Blocking send and receive message to a RPC service
 *
 * This is intended for custom RPC messages that don't follow the predefined
 * filesystem commands.  The kernel will prefix messages with a fsreq IOV with
 * cmd=CMD_SENDREC before being sent to the server. 
 */
int sys_sendrec(int fd, int siov_cnt, struct IOV *siov, int riov_cnt, struct IOV *riov)
{
  return -ENOSYS;
}

 
/* @brief   Send a message to a message port and wait for a reply.
 *
 * TODO:  Need timeout and abort mechanisms into IPC in case server doesn't respond or terminates.
 * Also timeout works for when a server tries to create a second mount in its own mount path.
 */
int ksendmsg(struct MsgPort *msgport, int siov_cnt, struct IOV *siov, int riov_cnt, struct IOV *riov)
{
  struct Process *current;
  struct Msg msg;
  int msgid;
  
  current = get_current_process();
  
  msg.msgid = new_msgid();  
  hash_msg(&msg);
    
  msg.reply_port = &current->reply_port;  
  msg.siov_cnt = siov_cnt;
  msg.siov = siov;
  msg.riov_cnt = riov_cnt;
  msg.riov = riov;  
  msg.reply_status = 0;
     
  kputmsg(msgport, &msg);   
  kwaitport(&current->reply_port, NULL);  
  kgetmsg(&current->reply_port);
  unhash_msg(&msg);

  return msg.reply_status;
}


/* @brief   Send a message to a message port but do not wait
 *
 * The calling function must already allocate and set the msgid of the message
 * msg already has the msg.msgid set.
 */
int kputmsg(struct MsgPort *msgport, struct Msg *msg)
{
  msg->port = msgport;
  LIST_ADD_TAIL(&msgport->pending_msg_list, msg, link);

  knote(&msgport->knote_list, NOTE_MSG);  

  return 0;
}


/* @brief   Reply to a kernel message
 *
 * Note: The msgid in the message must be freed after calling kreplymsg.
 */
int kreplymsg(struct Msg *msg)
{
  struct MsgPort *reply_port;

  KASSERT (msg != NULL);  
  KASSERT (msg->reply_port != NULL);
  
  msg->port = msg->reply_port;
  reply_port = msg->reply_port;
  LIST_ADD_TAIL(&reply_port->pending_msg_list, msg, link);
  TaskWakeup(&reply_port->rendez);
  return 0;  
}


/* @brief   Get a kernel message from a message port
 */
struct Msg *kgetmsg(struct MsgPort *msgport)
{
  struct Msg *msg;
    
  msg = LIST_HEAD(&msgport->pending_msg_list);
  
  if (msg) {
    LIST_REM_HEAD(&msgport->pending_msg_list, link);
  }
  
  return msg;
}


/* @brief   Wait for a message port to receive a message
 *
 * @param   msgport, message port to wait on
 * @param   timeout, duration to wait for a message
 * @return  0 on success,
 *          -ETIMEDOUT on timeout
 *          negative errno on other failure
 */
int kwaitport(struct MsgPort *msgport, struct timespec *timeout)
{
  while (LIST_HEAD(&msgport->pending_msg_list) == NULL) {
    if (timeout == NULL) {
      TaskSleep(&msgport->rendez);
    } else {    
      if (TaskTimedSleep(&msgport->rendez, timeout) == -ETIMEDOUT) {
        return -ETIMEDOUT;  
      }
    }
  }

  return 0;  
}


/* @brief   Initialize a message port
 *
 * @param   msgport, message port to initialize
 * @return  0 on success, negative errno on error
 */
int init_msgport(struct MsgPort *msgport)
{
  LIST_INIT(&msgport->pending_msg_list);
  LIST_INIT(&msgport->knote_list);
  InitRendez(&msgport->rendez);
  msgport->context = NULL;
  return 0;
}


/* @brief   Cleanup a message port
 * 
 * @param   msgport, message port to cleanup
 * @return  0 on success, negative errno on error
 * 
 * TODO: Reply to any pending or in progress messages
 */
int fini_msgport(struct MsgPort *msgport) {
  return 0;
} 


/* @brief   Get a pointer to message from the message ID.
 *
 * @param   port, the message port this message was sent to
 * @param   msgid, message ID of the message to lookup.
 * @return  Pointer to message structure, or NULL on failure
 *
 * A server could possibly receive connections from thousands of client threads.
 * In this case it is up to the server to return -EAGAIN if it cannot handle that
 * many messages. This applies to character devices such as terminals and serial
 * ports that may have many threads performing reads, writes and ioctls. 
 */
struct Msg *msgid_to_msg(struct MsgPort *msgport, msgid_t msgid)
{
  struct Msg *msg;
  uint32_t hash;
  
  hash = msgid % MSGID_HASH_SZ;
  
  msg = LIST_HEAD(&msgid_hash[hash]);
  
  while (msg != NULL) {
    if (msg->msgid == msgid && msg->port == msgport) {
      break;
    }
    
    msg = LIST_NEXT(msg, link);
  }
  
  return msg;
}


/*
 *
 */
msgid_t new_msgid(void)
{
  return unique_msgid_counter++;
}


/*
 *
 */
void hash_msg(struct Msg *msg)
{
  int hash;
  
  hash = msg->msgid % MSGID_HASH_SZ;
  LIST_ADD_TAIL(&msgid_hash[hash], msg, link);
}


/*
 *
 */
void unhash_msg(struct Msg *msg)
{
  int hash;
  
  hash = msg->msgid % MSGID_HASH_SZ;
  LIST_REM_ENTRY(&msgid_hash[hash], msg, link);
}


/* @brief   Seek to a position within a multi-part message
 *
 */
int seekiov(int iov_cnt, struct IOV *iov, off_t offset, int *i, size_t *iov_remaining)
{
  off_t base_offset;

  base_offset = 0;
  
  for (*i = 0; *i < iov_cnt; (*i)++) {
    if (offset >= base_offset &&
        offset < base_offset + iov[*i].size) {
      *iov_remaining = base_offset + iov[*i].size - offset;
      break;
    }

    base_offset += iov[*i].size;
  }

  if (*i >= iov_cnt) {
    return -EINVAL;
  }

  return 0;
}


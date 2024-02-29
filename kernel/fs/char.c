/*
 * Copyright 2014  Marven Gilhespie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http: *www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define KDEBUG

#include <kernel/types.h>
#include <kernel/proc.h>
#include <kernel/filesystem.h>
#include <kernel/utility.h>
#include <kernel/vm.h>
#include <kernel/dbg.h>
#include <kernel/board/boot.h>
#include <kernel/globals.h>
#include <sys/termios.h>


/* @brief   Read data from a character device
 *
 * TODO: Currently using a 512 byte buffer on the stack.  We need to eliminate the double
 * copy from the character driver to this buffer on a vfs_read and then from this buffer
 * to the client's user-space buffer.
 *
 * Could server-side readmsg/writemsg access the physical memory map (to avoid double copy).
 * Add CopyFromProcess(), CopyToProcess() to do this instead of copyin, copyout.
 *
 * Note: There is no use of vnode->busy.  Instead we use vnode->reader_cnt and vnode->writer_cnt
 * All other commands are assumed to be going to the command type queue.
 *
 * This code limits the sending of 1 write, 1 read and 1 command at a time.
 *
 * TODO: Need to handle non-blocking reads and writes
 */
ssize_t read_from_char(struct VNode *vnode, void *dst, size_t sz)
{
  uint8_t buf[512];
  ssize_t xfered = 0;
  size_t xfer = 0;
  struct Process *current;
  
  Info("read_from_char(dst:%08x, sz:%d", (uint32_t)dst, sz);
  
  current = get_current_process();
  
  while (vnode->reader_cnt != 0) {
    TaskSleep(&vnode->rendez);
  }

  vnode->reader_cnt = 1;
    
  xfer = (sz < sizeof buf) ? sz : sizeof buf;

  if (xfer > 0) {
    xfered = vfs_read(vnode, buf, xfer, NULL);
   
    if (xfered > 0) {  
      CopyOut (dst, buf, xfered);
    }
  }
  
  vnode->reader_cnt = 0;
  TaskWakeupAll(&vnode->rendez);

  return xfered;
}


/* @brief   Write data to a character device
 *
 */
ssize_t write_to_char(struct VNode *vnode, void *src, size_t sz)
{
  uint8_t buf[512];
  ssize_t xfered = 0;
  size_t xfer = 0;
  struct Process *current;

  current = get_current_process();

  while (vnode->writer_cnt != 0) {
    TaskSleep(&vnode->rendez);
  }

  vnode->writer_cnt = 1;

  xfer = (sz < sizeof buf) ? sz : sizeof buf;

  if (xfer > 0) {      

    CopyIn (buf, src, xfer);
    xfered = vfs_write(vnode, buf, xfer, NULL);
  }
    
  vnode->writer_cnt = 0;
  TaskWakeupAll(&vnode->rendez);    

  return xfered;
}


/* @brief   Set attributes of terminal device
 *
 */
int sys_tcsetattr (int fd, struct termios *_termios)
{
  return -ENOSYS;
}


/* @brief   Get attributes of terminal device
 *
 */
int sys_tcgetattr (int fd, struct termios *_termios)
{
  return -ENOSYS;
}


/* @brief   Indicate if a file handle points to a TTY
 *
 */
int sys_isatty(int fd)
{
  // TODO: determine if handle is to a TTY instead of always true
  return 1;
}
 
 
/* @brief   Set the file handle to be used to receive kernel debug logs
 *
 */
int sys_set_syslog_file(int fd)
{
  return -ENOSYS;
}


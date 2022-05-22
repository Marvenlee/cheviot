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

#include <kernel/types.h>
#include <kernel/proc.h>
#include <kernel/filesystem.h>
#include <kernel/utility.h>
#include <kernel/vm.h>
#include <kernel/dbg.h>
#include <kernel/arm/boot.h>
#include <kernel/globals.h>
#include <sys/termios.h>



// TODO: Perhaps normal char/block reads and writes use 512 byte buffer on stack
// Add Fcntl option to change buffer size, possibly using allocated Buf

// Could server-side readmsg/writemsg access the physical memory map (to avoid double copy).

ssize_t ReadFromChar (struct VNode *vnode, void *dst, size_t sz) {
  uint8_t buf[512];
  ssize_t xfered = 0;
  size_t xfer = 0;
  struct Process *current;
  
  current = GetCurrentProcess();
  
  Info ("ReadFromChar(sz:%d)", sz);
  
  while (vnode->reader_cnt != 0) {
    TaskSleep(&vnode->rendez);
  }

  vnode->reader_cnt = 1;
    
  xfer = (sz < sizeof buf) ? sz : sizeof buf;

  if (xfer > 0) {
    xfered = vfs_read(vnode, buf, xfer, NULL);
   
    // TODO: Handle EOF ?   
    if (xfered > 0) {  
      CopyOut (dst, buf, xfered);
    }
  }
  
  vnode->reader_cnt = 0;
  TaskWakeupAll(&vnode->rendez);

  Info ("ReadFromChar xfered: %d", xfered);

  return xfered;
}


/*
 *
 */
ssize_t WriteToChar (struct VNode *vnode, void *src, size_t sz) {
  uint8_t buf[512];
  ssize_t xfered = 0;
  size_t xfer = 0;
  struct Process *current;
  
  current = GetCurrentProcess();

  while (vnode->writer_cnt != 0) {
    TaskSleep(&vnode->rendez);
  }

  vnode->writer_cnt = 1;

  xfer = (sz < sizeof buf) ? sz : sizeof buf;

  if (xfer > 0) {      

    // TODO: Handle EOF ?   

    CopyIn (buf, src, xfer);
    xfered = vfs_write(vnode, buf, xfer, NULL);
  }
    
  vnode->writer_cnt = 0;
  TaskWakeupAll(&vnode->rendez);    

  return xfered;
}



/*
 *
 */
SYSCALL int SysTCSetAttr (int fd, struct termios *_termios)
{
  // Allow only a single command at a time for this vnode
  Info ("TCSetAttr");
  
  return -ENOSYS;
}


/*
 *
 */
SYSCALL int SysTCGetAttr (int fd, struct termios *_termios)
{
  Info ("TCGetAttr");
  return -ENOSYS;
}


/*
 * Add IsATTY here
 */

SYSCALL int SysIsATTY(int fd)
{
  Info ("IsATTY always true, fd: %d", fd);
  return 1;
} 
 
/*
 * TODO: Drain functions
 */
 
 
 
 

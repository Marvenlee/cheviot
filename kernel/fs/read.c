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

#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/vm.h>
#include <sys/fsreq.h>
#include <sys/mount.h>


/* @brief   Read the contents of a file to a buffer
 *
 * @param   fd, file descriptor of file to read from
 * @param   dst, user-mode buffer to read data from file into
 * @param   sz, size in bytes of buffer pointed to by dst
 * @return  number of bytes read or negative errno on failure
 */
ssize_t sys_read(int fd, void *dst, size_t sz)
{
  struct Filp *filp;
  struct VNode *vnode;
  ssize_t xfered;
  struct Process *current;
  
  Info("\n\nsys_read");
  
  current = get_current_process();
  filp = get_filp(current, fd);
  vnode = get_fd_vnode(current, fd);

  if (vnode == NULL) {
    Error("sys_read fd:%d vnode null -EINVAL", fd);
    return -EBADF;
  }

  if (is_allowed(vnode, R_OK) != 0) {
    return -EACCES;
  }

#if 0   // FIXME: Check if reader permission  
  if (filp->flags & O_READ) == 0) {
    return -EACCES;
  } 
#endif  

  // Can VNode lock fail?  Can we do multiple readers/single writer ?
  vnode_lock(vnode);

  // Separate into vnode_ops structure for each device type

  if (S_ISCHR(vnode->mode)) {
    xfered = read_from_char (vnode, dst, sz);
  } else if (S_ISREG(vnode->mode)) {
    xfered = read_from_cache (vnode, dst, sz, &filp->offset, false);
  } else if (S_ISFIFO(vnode->mode)) {
    xfered = read_from_pipe (vnode, dst, sz);  
  } else if (S_ISBLK(vnode->mode)) {
    xfered = read_from_block (vnode, dst, sz, &filp->offset);
  } else if (S_ISDIR(vnode->mode)) {
    Error("sys_read fd:%d is a dir -EINVAL", fd);
    xfered = -EBADF;
  } else if (S_ISSOCK(vnode->mode)) {
    Error("sys_read fd:%d is a sock -EINVAL", fd);
    xfered = -EBADF;
  } else {
    Error("sys_read fd:%d is unknown -EINVAL", fd);
    xfered = -EBADF;
  }
  
  // Update accesss timestamps
  
  vnode_unlock(vnode);

  Info("sys_read done: %d\n", xfered);
  
  return xfered;
}


/* @brief   Read from a file to a kernel buffer
 *
 */
ssize_t kread(int fd, void *dst, size_t sz) {
  struct Filp *filp;
  struct VNode *vnode;
  ssize_t xfered;
  struct Process *current;
    
  current = get_current_process();
  filp = get_filp(current, fd);
  vnode = get_fd_vnode(current, fd);

  if (vnode == NULL) {
    return -EBADF;
  }
  
  if (is_allowed(vnode, R_OK) != 0) {
    return -EACCES;
  }

  vnode_lock(vnode);
  
  if (S_ISREG(vnode->mode)) {
    xfered = read_from_cache (vnode, dst, sz, &filp->offset, true);
  } else {
    xfered = -EBADF;
  }
  
  vnode_unlock(vnode);
  
  return xfered;
}



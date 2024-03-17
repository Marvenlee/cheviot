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


/* @brief   Write the contents of a buffer to a file
 *
 * @param   fd, file descriptor of file to write to
 * @param   src, user-mode buffer containing data to write to file
 * @param   sz, size in bytes of buffer pointed to by src
 * @return  number of bytes written or negative errno on failure
 */
ssize_t sys_write(int fd, void *src, size_t sz)
{
  struct Filp *filp;
  struct VNode *vnode;
  ssize_t xfered;
  struct Process *current;
  
  current = get_current_process();
  filp = get_filp(current, fd);
  vnode = get_fd_vnode(current, fd);

  if (vnode == NULL) {
    Error("sys_write fd:%d vnode null -EINVAL", fd);
    return -EINVAL;
  }

  if (is_allowed(vnode, W_OK) != 0) {
    return -EACCES;
  }
  
  #if 0   // FIXME: Check if writer permission  
  if (filp->flags & O_WRITE) == 0) {
    return -EACCES;
  } 
  #endif  

  
  vnode_lock(vnode);
  
  // TODO: Add write to cache path
  if (S_ISCHR(vnode->mode)) {
    xfered = write_to_char(vnode, src, sz);  
  } else if (S_ISBLK(vnode->mode)) {
    xfered = write_to_block(vnode, src, sz, &filp->offset);
  } else if (S_ISFIFO(vnode->mode)) {
    xfered = write_to_pipe(vnode, src, sz);
    Info("*** %d write_to_pipe(src:%08x, sz:%u)", xfered, (uint32_t)src, sz);
  } else {
    Error("sys_write fd:%d unknown type -EINVAL", fd);
    xfered = -EINVAL;
  }  

  // TODO: Update accesss timestamps
  vnode_unlock(vnode);
  
  return xfered;
}



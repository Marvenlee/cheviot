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


/*
 *
 */
SYSCALL ssize_t sys_read(int fd, void *dst, size_t sz) {
  struct Filp *filp;
  struct VNode *vnode;
  ssize_t xfered;

  Info("sys_read fd:%d, dst:%08x, sz:%d", fd, dst, sz);
  
  filp = get_filp(fd);

  if (filp == NULL) {
    return -EINVAL;
  }

  vnode = filp->vnode;

  if (is_allowed(vnode, R_OK) != 0) {
    return -EACCES;
  }

  // Can VNode lock fail?  Can we do multiple readers/single writer ?
  vnode_lock(vnode);

  // Needed for all vnode operations that can block, even briefly


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
    xfered = -EINVAL;
  } else {
    Info ("Read from unknown, -EINVAL (oct) %o", vnode->mode);
    xfered = -EINVAL;
  }
  
  // Update accesss timestamps
  
  vnode_unlock(vnode);
  
  Info ("sys_read xfered = %d",xfered);
  
  return xfered;
}

/*
 *
 */
ssize_t kread(int fd, void *dst, size_t sz) {
  struct Filp *filp;
  struct VNode *vnode;
  ssize_t xfered;
  
  Info("KRead fd=%d, sz=%d", fd, sz);
  
  filp = get_filp(fd);

  if (filp == NULL) {
    return -EINVAL;
  }

  vnode = filp->vnode;

  if (is_allowed(vnode, R_OK) != 0) {
    return -EACCES;
  }

  vnode_lock(vnode);
  
  if (S_ISREG(vnode->mode)) {
    xfered = read_from_cache (vnode, dst, sz, &filp->offset, true);
  } else {
    Info ("Read from unknown, -EINVAL (oct) %o", vnode->mode);
    xfered = -EINVAL;
  }
  
  vnode_unlock(vnode);
  
  return xfered;
}






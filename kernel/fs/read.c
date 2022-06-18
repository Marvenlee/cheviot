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
SYSCALL ssize_t SysRead(int fd, void *dst, size_t sz) {
  struct Filp *filp;
  struct VNode *vnode;
  ssize_t xfered;

  Info ("SysRead fd:%d, sz:%d", fd, sz);
  
  filp = GetFilp(fd);

  if (filp == NULL) {
    return -EINVAL;
  }

  vnode = filp->vnode;

  if (IsAllowed(vnode, R_OK) != 0) {
    return -EACCES;
  }

  // Can VNode lock fail?  Can we do multiple readers/single writer ?
  VNodeLock(vnode);

  // Needed for all vnode operations that can block, even briefly


  // Separate into vnode_ops structure for each device type

  if (S_ISCHR(vnode->mode)) {  
    xfered = ReadFromChar (vnode, dst, sz);
    Info("%d = ReadFromChar", xfered);
  } else if (S_ISREG(vnode->mode)) {
    xfered = ReadFromCache (vnode, dst, sz, &filp->offset, false);
  } else if (S_ISFIFO(vnode->mode)) {
//    xfered = ReadFromPipe (vnode, dst, sz, &filp->offset);  
  } else if (S_ISBLK(vnode->mode)) {
    xfered = ReadFromCache (vnode, dst, sz, &filp->offset, false);   // FIXME: Don't cache block devices
  } else if (S_ISDIR(vnode->mode)) {
    xfered = -EINVAL;
  } else {
    Info ("Read from unknown, -EINVAL (oct) %o", vnode->mode);
    xfered = -EINVAL;
  }
  
  // Update accesss timestamps
  
  VNodeUnlock(vnode);
  
  return xfered;
}

/*
 *
 */
ssize_t KRead(int fd, void *dst, size_t sz) {
  struct Filp *filp;
  struct VNode *vnode;
  ssize_t xfered;
  
  Info("KRead fd=%d, sz=%d", fd, sz);
  
  filp = GetFilp(fd);

  if (filp == NULL) {
    return -EINVAL;
  }

  vnode = filp->vnode;

  if (IsAllowed(vnode, R_OK) != 0) {
    return -EACCES;
  }

  VNodeLock(vnode);
  
  if (S_ISREG(vnode->mode)) {
    xfered = ReadFromCache (vnode, dst, sz, &filp->offset, true);
  } else {
    Info ("Read from unknown, -EINVAL (oct) %o", vnode->mode);
    xfered = -EINVAL;
  }
  
  VNodeUnlock(vnode);
  
  return xfered;
}






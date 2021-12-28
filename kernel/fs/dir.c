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
#include <kernel/utility.h>
#include <poll.h>
#include <string.h>

/*
 *
 */
SYSCALL int SysChDir(char *_path) {
  struct Process *current;
  struct Lookup lookup;
  int err;

  current = GetCurrentProcess();

  Info ("ChDir");

  if ((err = Lookup(_path, 0, &lookup)) != 0) {
    return err;
  }

  if (!S_ISDIR(lookup.vnode->mode)) {
    return -ENOTDIR;
  }

  if (current->current_dir != NULL) {
    VNodePut(current->current_dir);
  }

  VNodePut(current->current_dir);
  current->current_dir = lookup.vnode;
  VNodeUnlock(current->current_dir);
  return 0;
  
exit:
  VNodePut(lookup.vnode);
  return err;
}


/*
 *
 */
SYSCALL int SysFChDir(int fd) {
  struct Process *current;
  struct Filp *filp;
  struct VNode *vnode;

  Info ("FChDir %d", fd);

  current = GetCurrentProcess();

  filp = GetFilp(fd);

  if (filp == NULL) {
    return -EINVAL;
  }

  vnode = filp->vnode;

  if (!S_ISDIR(vnode->mode)) {
    return -ENOTDIR;
  }

  VNodePut(current->current_dir);
  current->current_dir = vnode;
  VNodeIncRef(vnode);
  return 0;
}


/*
 *
 */
SYSCALL int SysOpenDir(char *_path) {
  struct Process *current;
  struct Lookup lookup;
  int fd;
  struct Filp *filp = NULL;
  int err;

  current = GetCurrentProcess();

  if ((err = Lookup(_path, 0, &lookup)) != 0) {
    Warn("OpenDir lookup failed");
    return err;
  }

  if (!S_ISDIR(lookup.vnode->mode)) {
    err = -EINVAL;
    goto exit;
  }

  fd = AllocHandle();

  if (fd < 0) {
    err = -ENOMEM;
    goto exit;
  }

  filp = GetFilp(fd);
  filp->vnode = lookup.vnode;
  filp->offset = 0;
  VNodeUnlock(filp->vnode);

  Info ("OpenDir handle %d", fd);
  return fd;

exit:
    FreeHandle(fd);
    VNodePut(lookup.vnode);
    return -ENOMEM;
}


/*
 *
 */
void InvalidateDir(struct VNode *dvnode) {
  // Call when creating or deleting entries in dvnode.
  // Or deleting the actual directory itself.
}


/*
 * Initially make buf permanent, (until we can somehow unwire it/put it on the
 * reusable list).
 *
 *
 */
SYSCALL ssize_t SysReadDir(int fd, void *dst, size_t sz) {
  struct Filp *filp = NULL;
  struct VNode *vnode = NULL;
  ssize_t dirents_sz;
  off64_t cookie;
  uint8_t buffer[512];
  
  
  filp = GetFilp(fd);

  if (filp == NULL) {
    Info ("K Readdir EINVAL");
    return -EINVAL;
  }

  vnode = filp->vnode;

  if (!S_ISDIR(vnode->mode)) {
    Info ("K Readdir ENOTDIR");
    return -ENOTDIR;
  }
  
  cookie = filp->offset;

  VNodeLock(vnode);
  dirents_sz = vfs_readdir(vnode, buffer, sizeof buffer, &cookie);

  if (dirents_sz > 0) {  
    CopyOut(dst, buffer, dirents_sz);
  }

  filp->offset = cookie;
  VNodeUnlock(vnode);
  return dirents_sz;
}



SYSCALL int SysRewindDir(int fd) {
  struct Filp *filp = NULL;
  struct VNode *vnode = NULL;

  filp = GetFilp(fd);

  if (filp == NULL) {
    return -EINVAL;
  }

  vnode = filp->vnode;

  if (!S_ISDIR(vnode->mode)) {
    return -EINVAL;
  }

  filp->offset = 0;
  return 0;
}

SYSCALL int SysCreateDir(char *_path, mode_t mode) {
  struct Process *current;
  struct VNode *dvnode = NULL;
  struct VNode *vnode = NULL;
  struct Filp *filp = NULL;
  int err = 0;
  struct Lookup lookup;
  struct stat stat;

  Info("CreateDir");

  current = GetCurrentProcess();

  if ((err = Lookup(_path, LOOKUP_PARENT, &lookup)) != 0) {
    Warn("Lookup failed");
    goto exit;
  }

  vnode = lookup.vnode;
  dvnode = lookup.parent; // is this returning "dev" or "dev/" the mount point?

  KASSERT(dvnode != NULL);

  if (vnode == NULL) {
    err = vfs_mkdir(dvnode, lookup.last_component, &stat, &vnode);
    if (err != 0) {
      goto exit;
    }
  } else {
    // already exists, check if it is a directory
    if (!S_ISDIR(vnode->mode)) {
      err = -ENOTDIR;
      goto exit;
    }
  }

  WakeupPolls(dvnode, POLLPRI, POLLPRI);
  VNodePut (vnode);
  VNodePut (dvnode);
  return 0;

exit:
  VNodePut (vnode);
  VNodePut (dvnode);
  return err;
}


SYSCALL int SysRemoveDir(char *_path) {
  struct VNode *vnode = NULL;
  struct VNode *dvnode = NULL;
  int err = 0;
  struct Lookup lookup;

  Info ("RemoveDir");

  if ((err = Lookup(_path, LOOKUP_REMOVE, &lookup)) != 0) {
    Warn("Lookup failed");
    return err;
  }

  vnode = lookup.vnode;
  dvnode = lookup.parent;

  if (!S_ISDIR(vnode->mode)) {
    err = -ENOTDIR;
    goto exit;
  }

  vnode->reference_cnt--;

  if (vnode->reference_cnt == 0) // us incremented
  {
    vfs_rmdir(dvnode, lookup.last_component);
  }

  WakeupPolls(dvnode, POLLPRI, POLLPRI);
  VNodePut(vnode); // This should delete it
  VNodePut(dvnode);
  return 0;

exit:  
  VNodePut(vnode); // This should delete it
  VNodePut(dvnode);
  return err;
}




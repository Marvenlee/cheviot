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
SYSCALL int sys_chdir(char *_path)
{
  struct Process *current;
  struct lookupdata ld;
  int err;

  current = get_current_process();

  Info ("ChDir");

  if ((err = lookup(_path, 0, &ld)) != 0) {
    Info ("Chdir lookup err:%d", err);
    return err;
  }

  if (!S_ISDIR(ld.vnode->mode)) {
    Info ("ChDir -ENOTDIR");
    vnode_put(ld.vnode);
    return -ENOTDIR;
  }

  if (current->current_dir != NULL) {
    Info ("ChDir - current_Dir != NULL");
    vnode_put(current->current_dir);
  }

  current->current_dir = ld.vnode;
  vnode_unlock(current->current_dir);
  return 0;
}


/*
 *
 */
SYSCALL int sys_fchdir(int fd)
{
  struct Process *current;
  struct Filp *filp;
  struct VNode *vnode;

  Info ("FChDir %d", fd);

  current = get_current_process();

  filp = get_filp(fd);

  if (filp == NULL) {
    return -EINVAL;
  }

  // FIXME: Do we need to lock vnode if we don't block ?

  vnode = filp->vnode;

  if (!S_ISDIR(vnode->mode)) {
    return -ENOTDIR;
  }

  Info ("SysFChdir vnode_put");
  vnode_put(current->current_dir);
  current->current_dir = vnode;
  vnode_inc_ref(vnode);
  return 0;
}


/*
 *
 */
SYSCALL int sys_opendir(char *_path)
{
  struct Process *current;
  struct lookupdata ld;
  int fd;
  struct Filp *filp = NULL;
  int err;

  Info("SysOpenDir");

  current = get_current_process();

  if ((err = lookup(_path, 0, &ld)) != 0) {
    Warn("OpenDir lookup failed");
    return err;
  }

  if (!S_ISDIR(ld.vnode->mode)) {
    err = -EINVAL;
    goto exit;
  }

  fd = alloc_fd();

  if (fd < 0) {
    err = -ENOMEM;
    goto exit;
  }

  filp = get_filp(fd);
  filp->vnode = ld.vnode;
  filp->offset = 0;
  vnode_unlock(filp->vnode);

  Info ("OpenDir handle %d", fd);
  return fd;

exit:
    free_fd(fd);
    Info ("SysOpenDir vnode_put");
    vnode_put(ld.vnode);
    return -ENOMEM;
}


/*
 *
 */
void invalidate_dir(struct VNode *dvnode)
{
  // Call when creating or deleting entries in dvnode.
  // Or deleting the actual directory itself.
}


/*
 * Initially make buf permanent, (until we can somehow unwire it/put it on the
 * reusable list).
 *
 *
 */
SYSCALL ssize_t sys_readdir(int fd, void *dst, size_t sz)
{
  struct Filp *filp = NULL;
  struct VNode *vnode = NULL;
  ssize_t dirents_sz;
  off64_t cookie;
  uint8_t buffer[512];    // FIXME: add constant name, 255 is max filename length, 512 reasonable size for multiple dirents 

  Info("SysReaddir");
  
  filp = get_filp(fd);

  if (filp == NULL) {
    Info ("Readdir EINVAL");
    return -EINVAL;
  }

  vnode = filp->vnode;

  if (!S_ISDIR(vnode->mode)) {
    Info ("Readdir ENOTDIR");
    return -ENOTDIR;
  }
  
  cookie = filp->offset;

  vnode_lock(vnode);
  dirents_sz = vfs_readdir(vnode, buffer, sizeof buffer, &cookie);

  if (dirents_sz > 0) {  
    CopyOut(dst, buffer, dirents_sz);
  }

  filp->offset = cookie;
  vnode_unlock(vnode);
  return dirents_sz;
}



SYSCALL int sys_rewinddir(int fd)
{
  struct Filp *filp = NULL;
  struct VNode *vnode = NULL;

  filp = get_filp(fd);

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

SYSCALL int sys_createdir(char *_path, mode_t mode)
{
  struct Process *current;
  struct VNode *dvnode = NULL;
  struct VNode *vnode = NULL;
  struct Filp *filp = NULL;
  int err = 0;
  struct lookupdata ld;
  struct stat stat;

  current = get_current_process();

  if ((err = lookup(_path, LOOKUP_PARENT, &ld)) != 0) {
    Warn("Lookup failed");
    goto exit;
  }

  vnode = ld.vnode;
  dvnode = ld.parent; // is this returning "dev" or "dev/" the mount point?

  KASSERT(dvnode != NULL);

  if (vnode == NULL) {
    err = vfs_mkdir(dvnode, ld.last_component, &stat, &vnode);
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

  wakeup_polls(dvnode, POLLPRI, POLLPRI);
  vnode_put (vnode);
  vnode_put (dvnode);
  return 0;

exit:
  vnode_put (vnode);
  vnode_put (dvnode);
  return err;
}


SYSCALL int sys_rmdir(char *_path) {
  struct VNode *vnode = NULL;
  struct VNode *dvnode = NULL;
  int err = 0;
  struct lookupdata ld;

  if ((err = lookup(_path, LOOKUP_REMOVE, &ld)) != 0) {
    Warn("Lookup failed");
    return err;
  }

  vnode = ld.vnode;
  dvnode = ld.parent;

  if (!S_ISDIR(vnode->mode)) {
    err = -ENOTDIR;
    goto exit;
  }

  vnode->reference_cnt--;

  if (vnode->reference_cnt == 0) // us incremented
  {
    vfs_rmdir(dvnode, ld.last_component);
  }

  wakeup_polls(dvnode, POLLPRI, POLLPRI);
  vnode_put(vnode); // This should delete it
  vnode_put(dvnode);
  return 0;

exit:  
  vnode_put(vnode); // This should delete it
  vnode_put(dvnode);
  return err;
}




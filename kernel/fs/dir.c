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
#include <kernel/kqueue.h>


/* @brief   Change the current directory
 *
 * @param   _path, directory pathname to change to
 * @return  0 on success, negative errno on failure
 */
int sys_chdir(char *_path)
{
  struct Process *current;
  struct lookupdata ld;
  int err;

  current = get_current_process();


  if ((err = lookup(_path, 0, &ld)) != 0) {
    return err;
  }

  if (!S_ISDIR(ld.vnode->mode)) {
    vnode_put(ld.vnode);
    return -ENOTDIR;
  }

  if (current->fproc->current_dir != NULL) {
    vnode_put(current->fproc->current_dir);
  }

  current->fproc->current_dir = ld.vnode;
  vnode_unlock(current->fproc->current_dir);
  return 0;
}


/* @brief   Change the current directory to that referenced by a file handle
 *
 * @param   fd, file handle to an existing open directory
 * @return  0 on success, negative errno on failure
 */
int sys_fchdir(int fd)
{
  struct Process *current;
  struct VNode *vnode;


  current = get_current_process();
  vnode = get_fd_vnode(current, fd);

  if (vnode == NULL) {
    return -EINVAL;
  }

  if (!S_ISDIR(vnode->mode)) {
    return -ENOTDIR;
  }

  vnode_put(current->fproc->current_dir);
  current->fproc->current_dir = vnode;
  vnode_inc_ref(vnode);
  return 0;
}


/* @brief   Change the root directory of the current process
 */ 
int sys_chroot(char *_new_root)
{
  return -ENOSYS;
}


/* @brief   Open a directory for reading
 * 
 * @param   _path, the pathname of a directory to open
 * @return  0 on success, negative errno on failure
 */
int sys_opendir(char *_path)
{
  struct Process *current;
  struct lookupdata ld;
  int fd;
  struct Filp *filp = NULL;
  int err;


  current = get_current_process();

  if ((err = lookup(_path, 0, &ld)) != 0) {
    return err;
  }

  if (!S_ISDIR(ld.vnode->mode)) {
    err = -EINVAL;
    goto exit;
  }

  fd = alloc_fd_filp(current);

  if (fd < 0) {
    err = -ENOMEM;
    goto exit;
  }

  filp = get_filp(current,fd);
  filp->type = FILP_TYPE_VNODE;
  filp->u.vnode = ld.vnode;
  filp->offset = 0;
  vnode_unlock(filp->u.vnode);

  return fd;

exit:
    free_fd_filp(current, fd);
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


/* @brief   Read the contents of a directory into a buffer
 * 
 * @param   fd, file handle to a directory opened by sys_opendir()
 * @param   dst, pointer to user-mode buffer to read directory entries into
 * @param   sz, size of user-mode buffer
 * @return  number of bytes read, 0 indicates end of directory, negative errno values on failure
 *
 * TODO: Add 4K dir cache in kernel if entire directory is less than 4K. Otherwise readthru
 * to the FS handler.  rmdir, rm etc should invalidate dir cache.
 * 
 * TODO: Need direct copy/page remap from fs handler server to client user-space without the
 * need for a buffer in the kernel, allows for bigger buffers.
 */
ssize_t sys_readdir(int fd, void *dst, size_t sz)
{
  struct Filp *filp = NULL;
  struct VNode *vnode = NULL;
  ssize_t dirents_sz;
  off64_t cookie;
  uint8_t dirbuf[512];    // FIXME: Replace with inter-address-space direct copy
  struct Process *current;

  current = get_current_process();
  filp = get_filp(current, fd);
  vnode = get_fd_vnode(current, fd);

  if (vnode == NULL) {
    return -EINVAL;
  }

  if (!S_ISDIR(vnode->mode)) {
    return -ENOTDIR;
  }
  
  cookie = filp->offset;

  vnode_lock(vnode);
  dirents_sz = vfs_readdir(vnode, dirbuf, sizeof dirbuf, &cookie);

  if (dirents_sz > 0) {
    CopyOut(dst, dirbuf, dirents_sz);
  }
  
  filp->offset = cookie;
  
  Info("sys_readdir, calling vnode_unlock");
  vnode_unlock(vnode);
  
  Info("sys_readdir, ret %d", dirents_sz);  
  return dirents_sz;
}


/* @brief   Seek to the beginning of a directory
 *
 * @param   fd, file handle to directory opened with opendir()
 * @return  0 on success, negative errno on failure
 */
int sys_rewinddir(int fd)
{
  struct Filp *filp = NULL;
  struct VNode *vnode = NULL;
  struct Process *current;
  
  current = get_current_process();
  filp = get_filp(current, fd);
  vnode = get_fd_vnode(current, fd);

  if (vnode == NULL) {
    return -EINVAL;
  }

  if (!S_ISDIR(vnode->mode)) {
    return -EINVAL;
  }

  filp->offset = 0;
  return 0;
}


/* @brief   Create a new directory
 *
 * @param   _path, pathname of new directory to create
 * @param   mode, ownership permissions of the new directory
 * @return  0 on success, negative errno on failure
 */
int sys_createdir(char *_path, mode_t mode)
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

  knote(&dvnode->knote_list, NOTE_WRITE | NOTE_ATTRIB);
  vnode_put (vnode);
  vnode_put (dvnode);
  return 0;

exit:
  vnode_put (vnode);
  vnode_put (dvnode);
  return err;
}


/* @brief   Remove an existing directory
 * 
 * @param   _path, pathname of the directory to delete
 * @return  0 on success, negative errno on failure
 */
int sys_rmdir(char *_path) {
  struct VNode *vnode = NULL;
  struct VNode *dvnode = NULL;
  int err = 0;
  struct lookupdata ld;

  if ((err = lookup(_path, LOOKUP_REMOVE, &ld)) != 0) {
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

  knote(&dvnode->knote_list, NOTE_WRITE | NOTE_ATTRIB);
  vnode_put(vnode); // This should delete it
  vnode_put(dvnode);
  return 0;

exit:  
  vnode_put(vnode); // This should delete it
  vnode_put(dvnode);
  return err;
}



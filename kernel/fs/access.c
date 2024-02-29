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
#include <poll.h>
#include <string.h>
#include <fcntl.h>
#include <kernel/kqueue.h>

/* @brief   access system call
 * 
 */
int sys_access(char *pathname, mode_t permissions)
{
	// TODO: implement sys_access
  return F_OK;
}


/* @brief   umask system call
 *
 */
mode_t sys_umask (mode_t mode)
{
  mode_t old_mode;
  struct Process *current;
  
  current = get_current_process();
  
  old_mode = current->fproc->umask;
  current->fproc->umask = mode;
  return old_mode;
}


/* @brief chmod system call
 *
 */
int sys_chmod(char *_path, mode_t mode)
{
  struct Process *current;
  struct lookupdata ld;
  struct VNode *vnode;
  int err;

  current = get_current_process();

  if ((err = lookup(_path, 0, &ld)) != 0) {
    return err;
  }

  vnode = ld.vnode;

  if (vnode->uid == current->uid) {
    err = vfs_chmod(vnode, mode);
    
    if (err == 0) {
      vnode->mode = mode;
    }
  } else {
    err = EPERM;
  }

  knote(&vnode->knote_list, NOTE_ATTRIB);
  
  vnode_put(vnode);
  return err;
}


/* @brief   chown system call
 *
 */
int sys_chown(char *_path, uid_t uid, gid_t gid)
{
  struct Process *current;
  struct lookupdata ld;
  struct VNode *vnode;
  int err;

  current = get_current_process();

  if ((err = lookup(_path, 0, &ld)) != 0) {
    return err;
  }

  vnode = ld.vnode;

  if (vnode->uid == current->uid) {
    err = vfs_chown(vnode, uid, gid);
    
    if (err == 0) {
      vnode->uid = uid;
      vnode->gid = gid;
    }    
  } else {
    err = EPERM;
  }

  knote(&vnode->knote_list, NOTE_ATTRIB);
  vnode_put(vnode);
  return 0;
}


/* @brief 	fchmod system call
 *
 */
int sys_fchmod(int fd, mode_t mode)
{
  struct Process *current;
  struct VNode *vnode;
  int err;

  current = get_current_process();
  vnode = get_fd_vnode(current, fd);

  if (vnode == NULL) {
    return -EINVAL;
  }

  if (vnode->uid == current->uid) {
    err = vfs_chmod(vnode, mode);
    
    if (err == 0) {
      vnode->mode = mode;
    }
  } else {
    err = EPERM;
  }

  knote(&vnode->knote_list, NOTE_ATTRIB);
  
  vnode_put(vnode);
  return err;
}


/* @brief   fchown system call
 *
 * FIXME: does get_fd_vnode lock the vnode?
 */
int sys_fchown(int fd, uid_t uid, gid_t gid)
{
  struct Process *current;
  struct VNode *vnode;
  int err;

  current = get_current_process();
  vnode = get_fd_vnode(current, fd);

  if (vnode == NULL) {
    return -EINVAL;
  }

  if (vnode->uid == current->uid) {
    err = vfs_chown(vnode, uid, gid);
    
    if (err == 0) {
      vnode->uid = uid;
      vnode->gid = gid;
    }    
  } else {
    err = EPERM;
  }


  knote(&vnode->knote_list, NOTE_ATTRIB);
  vnode_put(vnode);
  return 0;
}


/* @brief   Check if an operation is allowed on a file
 * 
 * TODO:  Should this also check the filp's mode bits ? 
 * 
 * TODO:  What if group and other have more privileges than owner?
 * TODO:  Add root/administrator check (GID = 0 or 1 for admins)?
 * Admins can't access root files.
 */
int is_allowed(struct VNode *vnode, mode_t desired_access)
{
  mode_t perm_bits;
  int shift;
  struct Process *current;

	// FIXME : fix is_allowed
  return 0;
  
  
  desired_access &= (R_OK | W_OK | X_OK);

  current = get_current_process();

  if (current->uid == vnode->uid)
    shift = 6; /* owner */
  else if (current->gid == vnode->gid)
    shift = 3; /* group */
  else
    shift = 0; /* other */

  perm_bits = (vnode->mode >> shift) & (R_OK | W_OK | X_OK);

  if ((perm_bits | desired_access) != perm_bits) {
    Warn("IsAllowed failed ************");
    return -EACCES;
  }

  return 0;
}



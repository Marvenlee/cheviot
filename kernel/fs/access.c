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

/*
 * TODO: Access
 */
SYSCALL int SysAccess(char *pathname, mode_t permissions) {
  // Open vnode,  call IsAllowed.
  // Is this with effective uid/gid ?
  Info ("Access mode:%d (dec)", permissions);
  return F_OK;
}


/*
 *
 */
SYSCALL mode_t SysUmask (mode_t mode) {
  mode_t old_mode;
  struct Process *current;
  
  current = GetCurrentProcess();
  
  old_mode = current->default_mode;
  current->default_mode = mode;
  
  Info ("Umask mode = %d, old_mode = %d", mode, old_mode);
  
  return old_mode;
}


/*
 *
 */
SYSCALL int SysChmod(char *_path, mode_t mode) {
  struct Process *current;
  struct Lookup lookup;
  struct VNode *vnode;
  int err;

  Info ("Chmod");

  current = GetCurrentProcess();

  if ((err = Lookup(_path, 0, &lookup)) != 0) {
    return err;
  }

  vnode = lookup.vnode;

  if (vnode->uid == current->uid) {
    err = vfs_chmod(vnode, mode);
    
    if (err == 0) {
      vnode->mode = mode;
    }
  } else {
    err = EPERM;
  }

  WakeupPolls(vnode, POLLPRI,POLLPRI);
  
  VNodePut(vnode);
  return err;
}


/*
 *
 */
SYSCALL int SysChown(char *_path, uid_t uid, gid_t gid) {
  struct Process *current;
  struct Lookup lookup;
  struct VNode *vnode;
  int err;

  Info ("Chown");

  current = GetCurrentProcess();

  if ((err = Lookup(_path, 0, &lookup)) != 0) {
    return err;
  }

  vnode = lookup.vnode;

  if (vnode->uid == current->uid) {
    err = vfs_chown(vnode, uid, gid);
    
    if (err == 0) {
      vnode->uid = uid;
      vnode->gid = gid;
    }    
  } else {
    err = EPERM;
  }

  WakeupPolls(vnode, POLLPRI, POLLPRI);
  VNodePut(vnode);
  return 0;
}


/*
 * TODO:  What if group and other have more privileges than owner?
 * TODO:  Add root/administrator check (GID = 0 or 1 for admins)?
 * Admins can't access root files.
 */
int IsAllowed(struct VNode *vnode, mode_t desired_access) {
  mode_t perm_bits;
  int shift;
  struct Process *current;

//  Info ("FIXME: IsAllowed vn = %08x, acc:%o", (vm_addr)vnode, desired_access);

// FIXME
  return 0;
  
  
  desired_access &= (R_OK | W_OK | X_OK);

  current = GetCurrentProcess();

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





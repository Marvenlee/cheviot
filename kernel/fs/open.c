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

// Static Prototypes
static int DoOpen(struct Lookup *lookup, int oflags, mode_t mode);

/*
 *
 */
SYSCALL int SysOpen(char *_path, int oflags, mode_t mode) {
  struct Lookup lookup;
  int sc;

  if ((sc = Lookup(_path, LOOKUP_PARENT, &lookup)) != 0) {
    Info ("Open - lookup failed, sc = %d", sc);
    return sc;
  }

  return DoOpen(&lookup, oflags, mode);
}

/*
 *
 */
int Kopen(char *_path, int oflags, mode_t mode) {
  struct Lookup lookup;
  int sc;

  if ((sc = Lookup(_path, LOOKUP_PARENT | LOOKUP_KERNEL, &lookup)) != 0) {
    Info ("Kopen - lookup failed, sc = %d", sc);
    return sc;
  }

  return DoOpen(&lookup, oflags, mode);
}

/*
 *
 */
static int DoOpen(struct Lookup *lookup, int oflags, mode_t mode) {
  struct Process *current;
  struct VNode *dvnode = NULL;
  struct VNode *vnode = NULL;
  int fd = -1;
  struct Filp *filp = NULL;
  int err = 0;
  struct stat stat;
  
  Info ("DoOpen");
  
  current = GetCurrentProcess();
  vnode = lookup->vnode;
  dvnode = lookup->parent;
  
  if (vnode == NULL) {
    if ((oflags & O_CREAT) && IsAllowed(dvnode, W_OK) != 0) {
      VNodePut(dvnode);
      return -ENOENT;
    }

    stat.st_mode = mode;
    stat.st_uid = current->uid;
    stat.st_gid = current->gid;

    if ((err = vfs_create(dvnode, lookup->last_component, oflags, &stat, &vnode)) != 0) {
      VNodePut(dvnode);      
      return err;
    }

    DNameEnter(dvnode, vnode, lookup->last_component);
  } else {
// FIXME:   if (vnode->vnode_mounted_here != NULL) {
      //	        VNodePut(vnode);
//      vnode = vnode->vnode_mounted_here;
      //	        VNodeLock (vnode);
//    }
  }

  VNodePut(dvnode);

  if (oflags & O_TRUNC) {
    if ((err = vfs_truncate(vnode, 0)) != 0) {
      VNodePut(vnode);
      return err;
    }
  }

  fd = AllocHandle();
  
  if (fd < 0) {
    err = -ENOMEM;
    goto exit;
  }

  filp = GetFilp(fd);
  filp->vnode = vnode;
  if (oflags & O_APPEND)
    filp->offset = vnode->size;
  else
    filp->offset = 0;

  VNodeUnlock(vnode);  
  return fd;
  
exit:
  FreeHandle(fd);
  VNodePut(vnode);
  return err;
}





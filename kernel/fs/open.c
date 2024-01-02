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

#define KDEBUG

#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/vm.h>
#include <poll.h>


// Static Prototypes
static int do_open(struct lookupdata *ld, int oflags, mode_t mode);


/*
 *
 */
int sys_open(char *_path, int oflags, mode_t mode)
{
  struct lookupdata ld;
  int sc;

  if ((sc = lookup(_path, LOOKUP_PARENT, &ld)) != 0) {
    Error("Open - lookup failed, sc = %d", sc);
    return sc;
  }

  return do_open(&ld, oflags, mode);
}

/*
 *
 */
int kopen(char *_path, int oflags, mode_t mode) {
  struct lookupdata ld;
  int sc;

  if ((sc = lookup(_path, LOOKUP_PARENT | LOOKUP_KERNEL, &ld)) != 0) {
    Error("Kopen - lookup failed, sc = %d", sc);
    return sc;
  }

  return do_open(&ld, oflags, mode);
}


/*
 *
 */
static int do_open(struct lookupdata *ld, int oflags, mode_t mode) {
  struct Process *current;
  struct VNode *dvnode = NULL;
  struct VNode *vnode = NULL;
  int fd = -1;
  struct Filp *filp = NULL;
  int err = 0;
  struct stat stat;
  
  current = get_current_process();
  vnode = ld->vnode;
  dvnode = ld->parent;
      
  if (vnode == NULL) {
    if ((oflags & O_CREAT) && is_allowed(dvnode, W_OK) != 0) {
      Error("SysOpen vnode_put O_CREAT");
      vnode_put(dvnode);
      return -ENOENT;
    }

    stat.st_mode = mode;
    stat.st_uid = current->uid;
    stat.st_gid = current->gid;

    if ((err = vfs_create(dvnode, ld->last_component, oflags, &stat, &vnode)) != 0) {
      Error("SysOpen vnode_put vfs_create");
      vnode_put(dvnode);      
      return err;
    }

//    DNameEnter(dvnode, vnode, ld->last_component);
  } else {
// FIXME:   if (vnode->vnode_mounted_here != NULL) {
      //	        vnode_put(vnode);
//      vnode = vnode->vnode_mounted_here;
      //	        vnode_lock (vnode);
//    }
  }

  vnode_put(dvnode);

  if (oflags & O_TRUNC) {
    if ((err = vfs_truncate(vnode, 0)) != 0) {
      Error("SysOpen vnode_put vfs_truncate");
      vnode_put(vnode);
      return err;
    }
  }

  fd = alloc_fd_filp(current);
  
  if (fd < 0) {
    err = -ENOMEM;
    goto exit;
  }

  filp = get_filp(current, fd);
  filp->type = FILP_TYPE_VNODE;
  filp->u.vnode = vnode;
  
  if (oflags & O_APPEND)
    filp->offset = vnode->size;
  else
    filp->offset = 0;

  vnode_unlock(vnode);
  return fd;
  
exit:
  Error("DoOpen failed: %d", err);
  free_fd_filp(current, fd);

  Error("SysOpen vnode_put exit:");
  vnode_put(vnode);
  return err;
}





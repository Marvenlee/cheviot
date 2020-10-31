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
#include <kernel/vm.h>
#include <kernel/utility.h>
#include <poll.h>

/*
 *
 */ 
int MkNod(char *_path, uint32_t flags, struct stat *_stat) {
  struct Lookup lookup;
  struct stat stat;
  struct Process *current;
  int fd = -1;
  int status = -ENOTSUP;
  struct VNode *vnode = NULL;
  
  current = GetCurrentProcess();

  if (CopyIn(&stat, _stat, sizeof stat) != 0) {
    return -EFAULT;
  }

  if ((status = Lookup(_path, LOOKUP_PARENT, &lookup)) != 0) {
    return status;
  }
  
  if (lookup.vnode != NULL)
  {    
    VNodeUnlock(lookup.vnode);
    VNodeUnlock(lookup.parent);
    status = -EEXIST;
    goto exit;  
  }
    
  status = vfs_mknod(lookup.parent, lookup.last_component, &stat, &vnode);

  VNodeUnlock(lookup.parent);

exit:
  return status;
}

/*
 * Mount()
 *
 * TODO: Add a struct statfs parameter to control cacheability/maximum size etc of a mount
 */ 
int Mount(char *_path, uint32_t flags, struct stat *_stat) {
  struct Lookup lookup;
  struct stat stat;
  struct Process *current;
  int fd = -1;
  int error = -ENOTSUP;
  struct VNode *vnode_covered = NULL;
  struct VNode *server_vnode = NULL;
  struct VNode *client_vnode = NULL;
  struct Filp *filp = NULL;
  struct SuperBlock *sb = NULL;
  
  current = GetCurrentProcess();

  if (CopyIn(&stat, _stat, sizeof stat) != 0) {
    return -EFAULT;
  }

  // TODO: Check if root == NULL and this is first Mount("/")
  if (root_vnode == NULL) {
    return MountRoot(flags, &stat);
  }

  if ((error = Lookup(_path, 0, &lookup)) != 0) {
    return error;
  }
  
  vnode_covered = lookup.vnode;

  if (vnode_covered == NULL) {
    error = -ENOENT;
    goto exit;
  }

  // TODO: Check stat.st_mode against vnode.mode  TYPE.
  // What about permissions, keep covered permissions ?  

  sb = AllocSuperBlock();

  if (sb == NULL) {
    error = -ENOMEM;
    goto exit;
  }

  DNamePurgeVNode(vnode_covered);

  if (vnode_covered->vnode_covered != NULL) {
    error = -EEXIST;
    goto exit;
  }

  server_vnode = VNodeNew(sb, -1);

  if (server_vnode == NULL) {
    error = -ENOMEM;
    goto exit;
  }

  // TODO: Set Client VNODE to some parameter of stat
  client_vnode = VNodeNew(sb, 0);

  if (client_vnode == NULL) {
    error = -ENOMEM;
    goto exit;
  }

  InitRendez(&sb->rendez);
  InitMsgPort(&sb->msgport, server_vnode);
  sb->server_vnode = server_vnode; // FIXME: Needed?
  sb->root = client_vnode;         // FIXME: Needed?
  sb->flags = flags;
  sb->reference_cnt = 2;
  sb->busy = FALSE;

  client_vnode->flags = V_VALID | V_ROOT;
  client_vnode->reference_cnt = 1;
  client_vnode->uid = stat.st_uid;
  client_vnode->gid = stat.st_gid;
  client_vnode->mode = stat.st_mode;
  client_vnode->size = stat.st_size;
  
  server_vnode->flags = V_VALID;
  server_vnode->reference_cnt = 1;
  server_vnode->uid = current->uid;
  server_vnode->gid = current->gid;
  server_vnode->mode = 0777 | _IFPORT;
  server_vnode->size = 0;
  // TODO: initialize rest of fields

  fd = AllocHandle();

  if (fd < 0) {
    error = -ENOMEM;
    goto exit;
  }

  // TODO:  Check for root earlier, don't allow remount?
  if (vnode_covered == root_vnode) {
    client_vnode->vnode_covered = NULL;
    root_vnode = client_vnode;
  } else {
    client_vnode->vnode_covered = vnode_covered;
    vnode_covered->vnode_mounted_here = client_vnode;
  }

  WakeupPolls(vnode_covered, POLLPRI, POLLPRI);
  WakeupPolls(client_vnode, POLLPRI, POLLPRI);

  filp = GetFilp(fd);

  filp->vnode = server_vnode;
  filp->offset = 0;

  VNodeIncRef(vnode_covered);
  VNodeIncRef(client_vnode);
  VNodeIncRef(server_vnode);

  VNodeUnlock(vnode_covered);
  VNodeUnlock(server_vnode);
  VNodeUnlock(client_vnode);

  return fd;

exit:

  VNodePut(server_vnode); // FIXME: Not a PUT?  Removed below?
  FreeHandle(fd);
  VNodePut(lookup.vnode);
  VNodeFree(client_vnode);
  VNodeFree(server_vnode);
  FreeSuperBlock(sb);
  VNodePut(vnode_covered);
  return error;
}


/*
 * ~unmount()
 */
int Unmount(int fd, bool force) {
  // send umount to all nodes to flush data
  return -ENOSYS;
}

/*
 *
 */ 
int ChRoot(char *_new_root) {
  return -ENOSYS;
}

/*
 *
 */
int Remount(char *_new_path, char *_old_path) {
  struct Lookup lookup;
  struct VNode *new_vnode_covered;
  struct VNode *old_vnode_mounted_here;
  
  if (sc != 0) {
    goto exit;
  }
  
  if ((sc = Lookup(_new_path, 0, &lookup)) != 0) {
    goto exit;
  }

  new_vnode_covered = lookup.vnode;
  

  if ((sc = Lookup(_old_path, 0, &lookup)) != 0) {
    goto exit;
  }

  old_vnode_mounted_here = lookup.vnode;
  
  // Check if old is a mount covering a vnode
  
  // Check that new is a dir but not covered or covering
}

/*
 *
 */
int PivotRoot(char *_new_root, char *_old_root) {
  struct Lookup lookup;
  struct VNode *new_root_vnode;
  struct VNode *old_root_vnode;
  char path[PATH_MAX];
  int sc;

  // Lookup new root
  if ((sc = Lookup(_new_root, 0, &lookup)) != 0) {
    goto exit;
  }

  new_root_vnode = lookup.vnode;
  
  // Lookup old root
  if ((sc = Lookup(_old_root, 0, &lookup)) != 0) {
    goto exit;
  }
  
  old_root_vnode = lookup.vnode;


  // Ensure vnode_covered != NULL
//  old_dev_vnode_mounted_here->vnode_covered->vnode_mounted_here = NULL;
  
//  if (new_root_vnode->vnode_mounted_here != NULL) {
//    new_root_vnode = new_root_vnode->vnode_mounted_here;
//  }

//  old_root_vnode->vnode_mounted_here = root_vnode;
//  root_vnode->vnode_covered = old_root_vnode;
  root_vnode = new_root_vnode;
//  root_vnode->vnode_covered = NULL;

  DNamePurgeAll();
  sc = 0;
  
exit:
  
  VNodePut (old_root_vnode);
  VNodePut (new_root_vnode);
//  VNodePut (old_root_vnode_mounted_here);
//  VNodePut (new_root_vnode_covered);
  return sc;
}

/*
 * Special case of mounting root IFS file system.
 */
int MountRoot(uint32_t flags, struct stat *stat) {
  struct Process *current;
  int fd = -1;
  int error;
  struct VNode *server_vnode = NULL;
  struct VNode *client_vnode = NULL;
  struct Filp *filp = NULL;
  struct SuperBlock *sb = NULL;
  
  Info("MountRoot");
  current = GetCurrentProcess();

  sb = AllocSuperBlock();

  if (sb == NULL) {
    error = -ENOMEM;
    goto exit;
  }

  // Server vnode set to -1.
  server_vnode = VNodeNew(sb, -1);

  if (server_vnode == NULL) {
    error = -ENOMEM;
    goto exit;
  }

  // TODO: Set Client VNODE to some parameter of stat
  client_vnode = VNodeNew(sb, 0);

  if (client_vnode == NULL) {
    error = -ENOMEM;
    goto exit;
  }

  InitRendez(&sb->rendez);
  InitMsgPort(&sb->msgport, server_vnode);
  sb->server_vnode = server_vnode; // FIXME: Needed?
  sb->root = client_vnode;         // FIXME: Needed?
  sb->flags = flags;
  sb->reference_cnt = 2;
  sb->busy = FALSE;

  server_vnode->flags = V_VALID;
  server_vnode->reference_cnt = 1;
  server_vnode->uid = current->uid;
  server_vnode->gid = current->gid;
  server_vnode->mode = 0777 | _IFPORT;
  server_vnode->size = 0;

  client_vnode->flags = V_VALID | V_ROOT;
  client_vnode->reference_cnt = 1;
  client_vnode->uid = stat->st_uid;
  client_vnode->gid = stat->st_gid;
  client_vnode->mode = stat->st_mode;
  client_vnode->size = stat->st_size;

  // TODO: initialize rest of fields

  fd = AllocHandle();

  if (fd == -1) {
    error = -ENOMEM;
    goto exit;
  }

  client_vnode->vnode_covered = NULL;

  root_vnode = client_vnode;

  filp = GetFilp(fd);
  filp->vnode = server_vnode;
  filp->offset = 0;

  VNodeUnlock(server_vnode);
  VNodeUnlock(client_vnode);

  if (root_vnode != NULL) {
    TaskWakeup(&root_rendez);
  }

  TaskWakeup(&root_rendez);
  return fd;
  
exit:
  Info ("MountRoot FAILED!!!!!!!!!!!!!!!1");
  return error;
}



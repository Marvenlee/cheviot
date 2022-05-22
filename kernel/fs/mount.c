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
 * Make a node of a given type char, block, etc which can serve as a mount point
 */ 
SYSCALL int SysMkNod(char *_path, uint32_t flags, struct stat *_stat) {
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
 */ 
SYSCALL int SysMount(char *_path, uint32_t flags, struct stat *_stat) {
  struct Lookup lookup;
  struct stat stat;
  struct Process *current;
  struct VNode *vnode_covered = NULL;
  struct VNode *server_vnode = NULL;
  struct VNode *client_vnode = NULL;
  struct Filp *filp = NULL;
  struct SuperBlock *sb = NULL;
  int fd = -1;
  int error = -ENOTSUP;
  
  Info ("SysMount");
  
  current = GetCurrentProcess();

  if (CopyIn(&stat, _stat, sizeof stat) != 0) {
    return -EFAULT;
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
  // As in only mount directories on directories ?
  
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

  client_vnode->vnode_covered = vnode_covered;
  vnode_covered->vnode_mounted_here = client_vnode;   // Or should it be SuperBlock (rename to VFS?) mounted here?

  Info ("Mount client_vnode = %08x", client_vnode);
  Info ("Mount server_vnode = %08x", server_vnode);
  Info ("Mount root_vnode = %08x", root_vnode);
  Info ("Mount vnode covered = %08x", vnode_covered);
  Info ("Mount client sb=%08x", client_vnode->superblock);
  Info ("Mount server sb=%08x", server_vnode->superblock);
  
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

  Info ("sb = %08x", sb);
  Info ("SysMount fd = %d", fd);

  // TODO: As the mount has changed, may want to wakeup tasks polling it for changes

  return fd;

exit:

  // FIXME: Need to understand/cleanup what vnode get/put/free/ alloc? do

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
SYSCALL int SysUnmount(int fd, bool force) {
  // send umount to all nodes to flush data
  return -ENOSYS;
}

/*
 *
 */ 
SYSCALL int SysChRoot(char *_new_root) {
  // Change the root for the current process.
  return -ENOSYS;
}

/*
 * Move a mount point to another location.  SUPERUSER only.
 * This is used to move the /dev mount on the IFS to /DEV on the new root moumt 
 *
 * Alternatively we would have to handle '/dev' in PivotRoot
 */
SYSCALL int SysMoveMount(char *_new_path, char *_old_path) {
  struct Lookup lookup;
  struct VNode *new_vnode;
  struct VNode *old_vnode;
  int error;
  
  Info("SysMoveMount");
  
  if ((error = Lookup(_new_path, 0, &lookup)) != 0) {
    Info("Failed to find new path");
    goto exit;
  }

  new_vnode = lookup.vnode;
  
  if (new_vnode->vnode_mounted_here != NULL) {
    Info("new vnode already has mount\n");
    goto exit;
  }

  if ((error = Lookup(_old_path, 0, &lookup)) != 0) {
    Info("Failed to find old path");
    goto exit;
  }

  old_vnode = lookup.vnode;
    
  if (old_vnode->vnode_mounted_here == NULL) {
    Info("old vnode not a mount point\n");
    goto exit;
  }
  
  // Only support directories?
  // Check if old is a mount covering a vnode, is a root vnode  
  // Check that new is a dir but not covered or covering
    
  new_vnode->vnode_mounted_here = old_vnode->vnode_mounted_here;  
  new_vnode->vnode_mounted_here->vnode_covered = new_vnode;  
  old_vnode->vnode_mounted_here = NULL;
  
  
  
  VNodePut (old_vnode);   // release 
  VNodePut (new_vnode);   // release

  return 0;
  
exit:
  return error;
}

/*
 *
 */
SYSCALL int SysPivotRoot(char *_new_root, char *_old_root) {
  struct Lookup lookup;
  struct VNode *new_root_vnode;
  struct VNode *old_root_vnode;
  struct VNode *current_root_vnode;
  int sc;

  Info("!!!!!!!!!!!!! PivotRoot() !!!!!!!!!!!!");

  // TODO: Check these are directories (ROOT ones)

  if ((sc = Lookup(_new_root, 0, &lookup)) != 0) {
    Info ("PivotRoot lookup _new_root failed");
    return sc;
  }

  new_root_vnode = lookup.vnode;

  if (new_root_vnode == NULL) {
    Info ("PivotRoot failed new_root -ENOENT");
    return -ENOENT;
  }

  if ((sc = Lookup(_old_root, 0, &lookup)) != 0) {
    Info ("PivotRoot lookup _old_root failed");
    return sc;
  }

  old_root_vnode = lookup.vnode;

  if (old_root_vnode == NULL) {
    Info ("PivotRoot lookup _old_root -ENOENT");
    VNodePut(new_root_vnode);
    return -ENOENT;
  }

  current_root_vnode = root_vnode->vnode_mounted_here;
  
  old_root_vnode->vnode_mounted_here = current_root_vnode;
  current_root_vnode->vnode_covered = old_root_vnode;

  root_vnode->vnode_mounted_here = new_root_vnode;
  new_root_vnode->vnode_covered = root_vnode;

  // TODO: Do we need to do any reference counting tricks, esp for current_vnode?
  
  DNamePurgeAll();
  
  VNodePut (old_root_vnode);
  VNodePut (new_root_vnode);
  Info ("*********** PIVOT ROOT SUCCESS **********");

  return 0;
}


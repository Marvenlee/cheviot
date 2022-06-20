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
SYSCALL int sys_mknod(char *_path, uint32_t flags, struct stat *_stat) {
  struct lookupdata ld;
  struct stat stat;
  struct Process *current;
  int fd = -1;
  int status = -ENOTSUP;
  struct VNode *vnode = NULL;

  Info ("**** SysMkNod ****");
  
  current = get_current_process();

  if (CopyIn(&stat, _stat, sizeof stat) != 0) {
    return -EFAULT;
  }

  if ((status = lookup(_path, LOOKUP_PARENT, &ld)) != 0) {
    return status;
  }

  Info ("MkNod lookupp complete");
  Info ("ld.parent = %08x", ld.parent);
  Info ("ld.vnode = %08x", ld.vnode);
  
  if (ld.vnode != NULL)
  {    
    vnode_put(ld.vnode);
    vnode_put(ld.parent);
    status = -EEXIST;
    goto exit;  
  }
    
  Info ("SysMkNod parent:08x", ld.parent);
  Info ("SysMkNod comp: %s", ld.last_component);
    
  status = vfs_mknod(ld.parent, ld.last_component, &stat, &vnode);

  vnode_put(vnode);
  vnode_put(ld.parent);
  
  Info ("SysMknod");

exit:
  return status;
}

/*
 * Mount()
 *
 */ 
SYSCALL int sys_mount(char *_path, uint32_t flags, struct stat *_stat) {
  struct lookupdata ld;
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
  
  current = get_current_process();

  if (CopyIn(&stat, _stat, sizeof stat) != 0) {
    return -EFAULT;
  }


  if (root_vnode != NULL) {
    if ((error = lookup(_path, 0, &ld)) != 0) {
      Info ("Mount lookup error:%d", error);
      return error;
    }
    
    Info ("Mount lookup ok");
    vnode_covered = ld.vnode;

    if (vnode_covered == NULL) {
      error = -ENOENT;
      goto exit;
    }

    // TODO: Check stat.st_mode against vnode.mode  TYPE.
    // As in only mount directories on directories ?
    
    // What about permissions, keep covered permissions ?  

    dname_purge_vnode(vnode_covered);

    if (vnode_covered->vnode_covered != NULL) {
      error = -EEXIST;
      goto exit;
    }

  } else {
    vnode_covered = NULL;  
  }


  sb = AllocSuperBlock();

  if (sb == NULL) {
    error = -ENOMEM;
    goto exit;
  }


  server_vnode = vnode_new(sb, -1);

  if (server_vnode == NULL) {
    error = -ENOMEM;
    goto exit;
  }

  client_vnode = vnode_new(sb, 0);

  if (client_vnode == NULL) {
    error = -ENOMEM;
    goto exit;    
  }

  InitRendez(&sb->rendez);
  init_msgport(&sb->msgport, server_vnode);
  sb->server_vnode = server_vnode; // FIXME: Needed?
  sb->root = client_vnode;         // FIXME: Needed?
  sb->flags = flags;
  sb->reference_cnt = 2;
  sb->busy = false;

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

  fd = alloc_fd();

  if (fd < 0) {
    error = -ENOMEM;
    goto exit;
  }


  Info ("Mount client_vnode = %08x", client_vnode);
  Info ("Mount server_vnode = %08x", server_vnode);
  Info ("Mount root_vnode = %08x", root_vnode);
  Info ("Mount vnode covered = %08x", vnode_covered);
  Info ("Mount client sb=%08x", client_vnode->superblock);
  Info ("Mount server sb=%08x", server_vnode->superblock);


  client_vnode->vnode_covered = vnode_covered;
  wakeup_polls(client_vnode, POLLPRI, POLLPRI);
  
  if (vnode_covered != NULL) {
    vnode_covered->vnode_mounted_here = client_vnode;   // Or should it be SuperBlock (rename to VFS?) mounted here?
    wakeup_polls(vnode_covered, POLLPRI, POLLPRI);
  }
  
  filp = get_filp(fd);

  filp->vnode = server_vnode;
  filp->offset = 0;

  if (root_vnode == NULL) {
    root_vnode = client_vnode;
  }    

  if (vnode_covered != NULL) {
    vnode_inc_ref(vnode_covered);
    vnode_unlock(vnode_covered);
  }

  vnode_inc_ref(client_vnode);
  vnode_inc_ref(server_vnode);

  vnode_unlock(server_vnode);
  vnode_unlock(client_vnode);

  Info ("sb = %08x", sb);
  Info ("SysMount fd = %d", fd);
  Info ("*************************************");
  
  // TODO: As the mount has changed, may want to wakeup tasks polling it for changes

  return fd;

exit:

  // FIXME: Need to understand/cleanup what vnode get/put/free/ alloc? do

  Info ("Mount: Vnodeput server_vnode");
  vnode_put(server_vnode); // FIXME: Not a PUT?  Removed below?
  free_fd(fd);

  Info ("Mount: Vnodeput ld.vnode");
  vnode_put(ld.vnode);
  
  vnode_free(client_vnode);
  vnode_free(server_vnode);
  FreeSuperBlock(sb);

  Info ("Mount: Vnodeput vnode_covered");  
  vnode_put(vnode_covered);
  return error;
}

/*
 * Do we need this as a syscall?
 */
SYSCALL int sys_unmount(int fd, bool force)
{
  return -ENOSYS;
}

/*
 *
 */ 
SYSCALL int sys_chroot(char *_new_root) {
  // Change the root for the current process.
  return -ENOSYS;
}

/*
 * Move a mount point to another location.  SUPERUSER only.
 * This is used to move the /dev mount on the IFS to /DEV on the new root moumt 
 *
 * Alternatively we would have to handle '/dev' in PivotRoot
 */
SYSCALL int sys_movemount(char *_new_path, char *_old_path) {
  struct lookupdata ld;
  struct VNode *new_vnode;
  struct VNode *old_vnode;
  int error;
  
  Info("SysMoveMount");
  
  if ((error = lookup(_new_path, 0, &ld)) != 0) {
    Info("Failed to find new path");
    goto exit;
  }

  new_vnode = ld.vnode;
  
  if (new_vnode->vnode_mounted_here != NULL) {
    Info("new vnode already has mount\n");
    goto exit;
  }

  if ((error = lookup(_old_path, 0, &ld)) != 0) {
    Info("Failed to find old path");
    goto exit;
  }

  old_vnode = ld.vnode;
    
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
  
  
  
  vnode_put (old_vnode);   // release 
  vnode_put (new_vnode);   // release

  return 0;
  
exit:
  return error;
}

/*
 *
 */
SYSCALL int sys_pivotroot(char *_new_root, char *_old_root) {
  struct lookupdata ld;
  struct VNode *new_root_vnode;
  struct VNode *old_root_vnode;
  struct VNode *current_root_vnode;
  int sc;

  Info("!!!!!!!!!!!!! PivotRoot() !!!!!!!!!!!!");

  // TODO: Check these are directories (ROOT ones)

  if ((sc = lookup(_new_root, 0, &ld)) != 0) {
    Info ("PivotRoot lookup _new_root failed");
    return sc;
  }

  new_root_vnode = ld.vnode;

  if (new_root_vnode == NULL) {
    Info ("PivotRoot failed new_root -ENOENT");
    return -ENOENT;
  }

  if ((sc = lookup(_old_root, 0, &ld)) != 0) {
    Info ("PivotRoot lookup _old_root failed");
    return sc;
  }

  old_root_vnode = ld.vnode;

  if (old_root_vnode == NULL) {
    Info ("PivotRoot lookup _old_root -ENOENT");
    vnode_put(new_root_vnode);
    return -ENOENT;
  }

  current_root_vnode = root_vnode;
  
  old_root_vnode->vnode_mounted_here = current_root_vnode;
  current_root_vnode->vnode_covered = old_root_vnode;

  root_vnode = new_root_vnode;
  new_root_vnode->vnode_covered = root_vnode;

  // TODO: Do we need to do any reference counting tricks, esp for current_vnode?
  
  dname_purge_all();
  
  vnode_put (old_root_vnode);
  vnode_put (new_root_vnode);
  Info ("*********** PIVOT ROOT SUCCESS **********");

  return 0;
}


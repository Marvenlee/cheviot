//#define KDEBUG

#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/vm.h>
#include <poll.h>


/*
 *
 */
 
int MkNod(char *_path, uint32_t flags, struct stat *_stat) {
  struct Lookup lookup;
  struct stat stat;
  struct Process *current;
  int handle = -1;
  int status = -ENOTSUP;
  struct VNode *vnode = NULL;
  
  Info("MkNod");
  
  current = GetCurrentProcess();

  if (CopyIn(&stat, _stat, sizeof stat) != 0) {
    return -EFAULT;
  }

  Info ("MkNod lookup, mode = %o", stat.st_mode);

  if ((status = Lookup(_path, LOOKUP_PARENT, &lookup)) != 0) {
    Info ("MkNod lookup failed");
    return status;
  }
  
  Info ("MkNod lookup complete");
  
  if (lookup.vnode != NULL)
  {
    Info ("MkNod lookup, vnode != NULL, exists");
    status = -EEXIST;
    goto exit;  
  }
    
  status = vfs_mknod(lookup.parent, lookup.last_component, &stat, &vnode);

  VNodeUnlock(lookup.parent);

exit:
  Info ("MkNod exit status = %d", status);  
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
  int handle = -1;
  int error = -ENOTSUP;
  struct VNode *vnode_covered = NULL;
  struct VNode *server_vnode = NULL;
  struct VNode *client_vnode = NULL;
  struct Filp *filp = NULL;
  struct SuperBlock *sb = NULL;
  
  Info("Mount");
  current = GetCurrentProcess();

  if (CopyIn(&stat, _stat, sizeof stat) != 0) {
    return -EFAULT;
  }

  Info("Mount mode = %o", stat.st_mode);


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
  sb->busy = false;

  client_vnode->flags = V_VALID | V_ROOT;
  client_vnode->reference_cnt = 1;
  client_vnode->uid = stat.st_uid;
  client_vnode->gid = stat.st_gid;
  client_vnode->mode = stat.st_mode;
  client_vnode->size = stat.st_size;
  
  Info (".. Mount client_vnode->mode = %o", client_vnode->mode);

  server_vnode->flags = V_VALID;
  server_vnode->reference_cnt = 1;
  server_vnode->uid = current->uid;
  server_vnode->gid = current->gid;
  server_vnode->mode = 0777 | _IFPORT;
  server_vnode->size = 0;


  // TODO: initialize rest of fields

  handle = AllocHandle(current);

  if (handle == -1) {
    error = -ENOMEM;
    goto exit;
  }

  filp = AllocFilp();

  if (filp == NULL) {
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

  Info ("mount - vnode_covered = %08x", vnode_covered);
  Info ("mount - vnode_mounted_here = %08x", client_vnode);

  filp->vnode = server_vnode;
  filp->offset = 0;
  SetHandle(current, handle, filp);

  VNodeIncRef(vnode_covered);
  VNodeIncRef(client_vnode);
  VNodeIncRef(server_vnode);

  VNodeUnlock(vnode_covered);
  VNodeUnlock(server_vnode);
  VNodeUnlock(client_vnode);

  Info("!! Mount succeeded !!");
  return handle;

exit:

  VNodePut(server_vnode); // FIXME: Not a PUT?  Removed below?

  FreeFilp(filp);
  FreeHandle(handle, current);
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

int PivotRoot(char *_new_root, char *_old_root) {
  struct Lookup lookup;
  struct VNode *new_root_vnode;
  struct VNode *old_root_vnode;
  int sc;

  Info("!!!!!!!!!!!!! PivotRoot() !!!!!!!!!!!!");

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

// TODO: Lookup /dev in old_root_vnode
// TODO: Lookup /dev in new_root_vnode

//  if (new_root_vnode->vnode_mounted_here != NULL) {
//    new_root_vnode = new_root_vnode->vnode_mounted_here;
//  }


//  old_root_vnode->vnode_mounted_here = root_vnode;
//  root_vnode->vnode_covered = old_root_vnode;
  root_vnode = new_root_vnode;
//  root_vnode->vnode_covered = NULL;
  DNamePurgeAll();
  
  VNodePut (old_root_vnode);
  VNodePut (new_root_vnode);
  Info ("*********** PIVOT ROOT SUCCESS **********");

  return 0;
}


/*
 * Special case of mounting root IFS file system.
 * FIXME: Why is this needed?
 */

int MountRoot(uint32_t flags, struct stat *stat) {
  struct Process *current;
  int handle = -1;
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
  sb->busy = false;

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

  handle = AllocHandle(current);

  if (handle == -1) {
    error = -ENOMEM;
    goto exit;
  }

  filp = AllocFilp();

  if (filp == NULL) {
    error = -ENOMEM;
    goto exit;
  }

  client_vnode->vnode_covered = NULL;

  root_vnode = client_vnode;

  filp->vnode = server_vnode;
  filp->offset = 0;


  SetHandle(current, handle, filp);

  VNodeUnlock(server_vnode);
  VNodeUnlock(client_vnode);

  if (root_vnode != NULL) {
    TaskWakeup(&root_rendez);
  }

  Info("!! Mount succeeded !!");


  TaskWakeup(&root_rendez);

  return handle;
  
exit:
  Info ("MountRoot FAILED!!!!!!!!!!!!!!!1");
  return error;
}











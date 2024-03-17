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
#include <kernel/vm.h>
#include <kernel/utility.h>
#include <poll.h>
#include <string.h>
#include <sys/mount.h>

/* @brief   Create a node on a filesystem
 */ 
int sys_mknod(char *_path, uint32_t flags, struct stat *_stat)
{
  struct lookupdata ld;
  struct stat stat;
  int sc;  
  struct VNode *vnode = NULL;

  if (CopyIn(&stat, _stat, sizeof stat) != 0) {
    return -EFAULT;
  }

  if ((sc = lookup(_path, LOOKUP_PARENT, &ld)) != 0) {
    return sc;
  }

  if (ld.vnode != NULL) {    
    vnode_put(ld.vnode);
    vnode_put(ld.parent);
    return -EEXIST;
  }
    
  sc = vfs_mknod(ld.parent, ld.last_component, &stat, &vnode);

  vnode_put(vnode);
  vnode_put(ld.parent);

  return sc;
}


/* @brief   Create a named msgport (mount point) in the file system namespace
 *
 * @param   _path, path to mount root of new filesystem at
 * @param   _stat, configuration settings to apply to this msgport (mount point).
 * @return  file descriptor on success, negative errno on failure
 *
 * This mounts a filesystem on top of an existing vnode. A file handle
 * is returned that points to this mount. A server can then use this
 * handle to receive and process messages from the kernel's virtual file
 * system. The sys_kevent, sys_getmsg, sys_replymsg, sys_readmsg and
 * sys_writemsg system calls are available to servers to process file
 * system requests.
 */ 
int sys_createmsgport(char *_path, uint32_t flags, struct stat *_stat, int backlog_sz)
{
  struct lookupdata ld;
  struct stat stat;
  struct Process *current;
  struct VNode *vnode_covered = NULL;
  struct VNode *mount_root_vnode = NULL;
  struct Filp *filp = NULL;
  struct SuperBlock *sb = NULL;
  struct MsgPort *msgport;
  int fd = -1;
  int error = -ENOTSUP;
  
  current = get_current_process();

  if (CopyIn(&stat, _stat, sizeof stat) != 0) {
    return -EFAULT;
  }

  if (root_vnode != NULL) {
    if ((error = lookup(_path, 0, &ld)) != 0) {
      return error;
    }

    vnode_covered = ld.vnode;

    if (vnode_covered == NULL) {
      error = -ENOENT;
      goto exit;
    }

    if (! ((S_ISDIR(stat.st_mode) && S_ISDIR(vnode_covered->mode))
        || (S_ISCHR(stat.st_mode) && S_ISCHR(vnode_covered->mode))
        || (S_ISBLK(stat.st_mode) && S_ISBLK(vnode_covered->mode)))) {
      errno = -EINVAL;
      goto exit;
    }
    
    // TODO: Check covered permissions ?  

    dname_purge_vnode(vnode_covered);

    if (vnode_covered->vnode_covered != NULL) {
      error = -EEXIST;
      goto exit;
    }

  } else { 
    // TODO: Check mount path is "/" only
    vnode_covered = NULL; 
  }

  fd = alloc_fd_superblock(current);
  
  if (fd < 0) {
    error = -ENOMEM;
    goto exit;
  }
  
  sb = get_superblock(current, fd);
  
  mount_root_vnode = vnode_new(sb, stat.st_ino);

  if (mount_root_vnode == NULL) {
    error = -ENOMEM;
    goto exit;    
  }

  InitRendez(&sb->rendez);

  init_msgport(&sb->msgport);
  init_msgbacklog(&sb->msgbacklog, backlog_sz);
  sb->msgport.context = sb;

  sb->root = mount_root_vnode;
  sb->flags = flags;
  sb->reference_cnt = 1;
  sb->busy = false;

  mount_root_vnode->flags = V_VALID | V_ROOT;
  mount_root_vnode->reference_cnt = 1;
  mount_root_vnode->uid = stat.st_uid;
  mount_root_vnode->gid = stat.st_gid;
  mount_root_vnode->mode = stat.st_mode;
  
  // TODO: dev_ stat.st_dev (major and minor numbers)
  // Check major/minor numbers, are allocated by current process.
  
  // TODO: Read-only filesystems don't need delayed writes
  if (S_ISDIR(mount_root_vnode->mode)) {
    init_superblock_bdflush(sb);
  }
  
  if (S_ISBLK(mount_root_vnode->mode)) {    
    mount_root_vnode->size = (off64_t)stat.st_blocks * (off64_t)stat.st_blksize;

    // FIXME: Could we use blocks and blkize instead of vnode->size?
    // Add these fields to vnode?
    //  blksize_t     st_blksize;
    //  blkcnt_t	st_blocks;
  
  } else {
    mount_root_vnode->size = stat.st_size;
  }
    
  mount_root_vnode->vnode_covered = vnode_covered;
  knote(&mount_root_vnode->knote_list, NOTE_ATTRIB);

  if (root_vnode == NULL) {
    root_vnode = mount_root_vnode;
    Info ("set root_vnode to: %08x", (uint32_t)root_vnode);
  }    
  
  if (vnode_covered != NULL) {
    vnode_covered->vnode_mounted_here = mount_root_vnode;
    knote(&vnode_covered->knote_list, NOTE_ATTRIB);

    vnode_inc_ref(vnode_covered);
    vnode_unlock(vnode_covered);
  }

  vnode_inc_ref(mount_root_vnode);
  vnode_unlock(mount_root_vnode);
  
  return fd;

exit:

  // FIXME: Need to understand/cleanup what vnode get/put/free/ alloc? do
  //  vnode_put(server_vnode); // FIXME: Not a PUT?  Removed below?
  //  free_msgportid(portid);

  Error("Mount: failed %d", error);

  vnode_put(ld.vnode);  
  vnode_free(mount_root_vnode);
  free_fd_superblock(current, fd);
  vnode_put(vnode_covered);
  return error;
}


/* @brief   Move a msgport to another location in the filesystem namespace.
 *
 * Used in conjunction with sys_pivotroot to move the /dev mount on the IFS 
 * to /dev on the new root moumt 
 */
int sys_renamemsgport(char *_new_path, char *_old_path)
{
  struct lookupdata ld;
  struct VNode *new_vnode;
  struct VNode *old_vnode;
  struct VNode *covered_vnode;
  int error;
  
  Info("sys_renamemsgport");
  
  if ((error = lookup(_new_path, 0, &ld)) != 0) {
    Error("Failed to find new path");
    goto exit;
  }

  new_vnode = ld.vnode;
  
  if (new_vnode->vnode_mounted_here != NULL) {
    Error("new vnode already has mount\n");
    goto exit;
  }

  if ((error = lookup(_old_path, 0, &ld)) != 0) {
    Error("Failed to find old path");
    goto exit;
  }

  old_vnode = ld.vnode;
    
  if (old_vnode->vnode_mounted_here == NULL) {
    if (old_vnode->vnode_covered == NULL) {
	    Error("old vnode not a mount point\n");
	    goto exit;
	  }
	  
	  covered_vnode = old_vnode->vnode_covered;

	  vnode_inc_ref(covered_vnode);
	  vnode_put(old_vnode);
	  old_vnode = covered_vnode;
	  

  }
  
  // Only support directories?
  // Check if old is a mount covering a vnode, is a root vnode  
  // Check that new is a dir but not covered or covering
    
  new_vnode->vnode_mounted_here = old_vnode->vnode_mounted_here;  
  new_vnode->vnode_mounted_here->vnode_covered = new_vnode;  
  old_vnode->vnode_mounted_here = NULL;
    
//  dname_purge_all();     FIXME:

  vnode_put (old_vnode);   // release 
  vnode_put (new_vnode);   // release

  Info("sys_movemount DONE");
  return 0;
  
exit:
  return error;
}


/* @brief   Pivot the root directory
 *
 */
int sys_pivotroot(char *_new_root, char *_old_root)
{
  struct lookupdata ld;
  struct VNode *new_root_vnode;
  struct VNode *old_root_vnode;
  struct VNode *current_root_vnode;
  int sc;

  Info("sys_pivotroot");
  // TODO: Check these are directories (ROOT ones)
	// TODO: Acquire old_root first, as this may be subdir of new root
	// and would cause deadlock due to new_root's busy flag being set.
	// Unless we unlock it (but it remains in memory due to ref cnt
	
  if ((sc = lookup(_old_root, 0, &ld)) != 0) {
    Error("PivotRoot lookup _old_root failed");
    return sc;
  }

  old_root_vnode = ld.vnode;


  if ((sc = lookup(_new_root, 0, &ld)) != 0) {
    Error("PivotRoot lookup _new_root failed");
    return sc;
  }

  new_root_vnode = ld.vnode;

  if (new_root_vnode == NULL) {
    Error("PivotRoot failed new_root -ENOENT");
    return -ENOENT;
  }

  if (old_root_vnode == NULL) {
    Error("PivotRoot lookup _old_root -ENOENT");
    vnode_put(new_root_vnode);
    return -ENOENT;
  }

  current_root_vnode = root_vnode;
  
  old_root_vnode->vnode_mounted_here = current_root_vnode;
  current_root_vnode->vnode_covered = old_root_vnode;

  root_vnode = new_root_vnode;
  new_root_vnode->vnode_covered = root_vnode;

  // TODO: Do we need to do any reference counting tricks, esp for current_vnode?
  
//  dname_purge_all();     FIXME:
  vnode_put (old_root_vnode);
  vnode_put (new_root_vnode);

  Info("sys_pivotroot DONE");
  
  return 0;
}


/*
 * Need to have separate function to sync the device,
 * flush all pending delayed writes and anything in message queue
 * Prevent further access
 */ 
int close_mount(struct Process *proc, int fd)
{
  struct SuperBlock *sb;
  struct VNode *vnode_covered;
  
  KASSERT(proc != NULL);
  
  sb = get_superblock(proc, fd);
  
  if (sb == NULL) {
    return -EINVAL;
  }

  /* FIXME: Check refernce count is zero when finally freeing
  
  vnode_covered = sb->vnode_covered;

  vnode_mounted_here = ;
  
  invalidate_superblock_blks(sb);
  invalidate_dnlc_entries(sb);
  invalidate_vnodes(sb);

  // TODO: Read-only filesystems don't need delayed writes

  if (S_ISDIR(vnode_mounted_here->mode)) {
    deinit_superblock_bdflush(sb);
  }

  vnode_covered->vnode_mounted_here = NULL;

  */
  
  free_fd_superblock(proc, fd);

  return 0;
}


/*
 * Get the superblock of a file descriptor created by sys_mount()
 */
struct SuperBlock *get_superblock(struct Process *proc, int fd)
{
  struct Filp *filp;

  KASSERT(proc != NULL);
  
  filp = get_filp(proc, fd);
    
  if (filp == NULL) {
    Error("get_superblock, filp is NULL");
    return NULL;
  }
  
  if (filp->type != FILP_TYPE_SUPERBLOCK) {
    Error("get_superblock, filp type is not SUPERBLOCK");
    return NULL;
  }
    
  return filp->u.superblock;
}


/*
 * Allocates a handle structure.  Checks to see that free_handle_cnt is
 * non-zero should already have been performed prior to calling alloc_fd().
 */
int alloc_fd_superblock(struct Process *proc)
{
  int fd;
  struct SuperBlock *sb;

  fd = alloc_fd_filp(proc);
  
  if (fd < 0) {
    Error("alloc_fd_superblock fd < 0");
    return -EMFILE;
  }
  
  sb = alloc_superblock();
  
  if (sb == NULL) {
    free_fd_filp(proc, fd);
    return -EMFILE;
  }
 
  set_fd(proc, fd, FILP_TYPE_SUPERBLOCK, 0, sb);  
  return fd;
}



/*
 * Returns a handle to the free handle list.
 */
int free_fd_superblock(struct Process *proc, int fd)
{
  struct SuperBlock *sb;
  
  sb = get_superblock(proc, fd);

  if (sb == NULL) {
    return -EINVAL;
  }

  free_superblock(sb);
  free_fd_filp(proc, fd);
  return 0;
}


/*
 *
 */
struct SuperBlock *alloc_superblock(void)
{
  struct SuperBlock *sb;

  sb = LIST_HEAD(&free_superblock_list);

  if (sb == NULL) {
    Error("no free superblocks");
    return NULL;
  }

  sb->reference_cnt = 1;

  LIST_REM_HEAD(&free_superblock_list, link);
  memset(sb, 0, sizeof *sb);
  InitRendez (&sb->rendez);
  
  sb->dev = sb - superblock_table;  
  return sb;
}


/*
 *
 */
void free_superblock(struct SuperBlock *sb)
{
  KASSERT (sb != NULL);

  sb->reference_cnt--;
  
  if (sb->reference_cnt == 0) {
    // TODO: Wakeup anything block on rendez ?
    LIST_ADD_TAIL(&free_superblock_list, sb, link);
  }
}







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
#include <kernel/proc.h>
#include <kernel/types.h>
#include <sys/mount.h>


/* @brief   Get the file statistics of a named file
 *
 * @param   _path, pathname to file to gather statistics of
 * @param   _stat, pointer to stat structure to return statistics
 * @return  0 on success, negative errno on error
 */
int sys_stat(char *_path, struct stat *_stat) {
  struct stat stat;
  struct lookupdata ld;
  int sc;

  Info("sys_stat");
	Info ("*** sizeof stat = %d", sizeof (struct stat));
	
  if ((sc = lookup(_path, 0, &ld)) != 0) {
    return sc;
  }
  
  Info("mode = %o", ld.vnode->inode_nr);

  stat.st_dev = ld.vnode->superblock->dev;
  stat.st_ino = ld.vnode->inode_nr;
  stat.st_mode = ld.vnode->mode;
  stat.st_nlink = ld.vnode->nlink;
  stat.st_uid = ld.vnode->uid;
  stat.st_gid = ld.vnode->gid;
  stat.st_rdev = ld.vnode->rdev;
  stat.st_size = ld.vnode->size;
  stat.st_atime = ld.vnode->atime;
  stat.st_mtime = ld.vnode->mtime;
  stat.st_ctime = ld.vnode->ctime;
  stat.st_blocks = ld.vnode->blocks;
  stat.st_blksize = ld.vnode->blksize;

  vnode_put(ld.vnode);

  if (_stat == NULL) {
    return -EFAULT;
  }

  if (CopyOut(_stat, &stat, sizeof stat) != 0) {
    return -EFAULT;
  }
  
  return 0;
}


/* @brief   Get the file statistics of an open file
 *
 * @param   fd, file handle of file to gather statistics of
 * @param   _stat, pointer to stat structure to return statistics
 * @return  0 on success, negative errno on error 
 */
int sys_fstat(int fd, struct stat *_stat) {
  struct Filp *filp;
  struct VNode *vnode;
  struct stat stat;
  struct Process *current;

  Info("sys_fstat fd:%d", fd);
	Info ("*** sizeof stat = %d", sizeof (struct stat));
	  
  current = get_current_process();
  filp = get_filp(current, fd);
  vnode = get_fd_vnode(current, fd);

  if (vnode == NULL) {
    Info("sys_fstat fd:%d no vnode, -EINVAL", fd);
    return -EINVAL;
  }

  vnode_lock(vnode);
  
  // FIXME: Should have vnodehold  when we know the vnode
  // But we don't acquire it , so no need for hold/put here.

  stat.st_dev = vnode->superblock->dev;
  stat.st_ino = vnode->inode_nr;
  stat.st_mode = vnode->mode;  
  stat.st_nlink = vnode->nlink;
  stat.st_uid = vnode->uid;
  stat.st_gid = vnode->gid;
  stat.st_rdev = vnode->rdev;
  stat.st_size = vnode->size;
  stat.st_atime = vnode->atime;
  stat.st_mtime = vnode->mtime;
  stat.st_ctime = vnode->ctime;
  stat.st_blocks = vnode->blocks;
  stat.st_blksize = vnode->blksize;

  vnode_unlock(vnode);

  if (_stat == NULL || CopyOut(_stat, &stat, sizeof stat) != 0) {
    return -EFAULT;
  }
  
  return 0;
}


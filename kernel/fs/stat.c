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

SYSCALL int sys_stat(char *_path, struct stat *_stat) {
  struct stat stat;
  struct lookupdata ld;
  int sc;

  Info ("SysStat");

  if ((sc = lookup(_path, 0, &ld)) != 0) {
    Info ("SysStat lookup: sc:%d", sc);
    return sc;
  }

  Info ("SysStat vnode:%08x", ld.vnode);  
  Info ("SysStat parent:%08x", ld.parent);

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

/*
 *
 */
SYSCALL int sys_fstat(int fd, struct stat *_stat) {
  struct Filp *filp;
  struct VNode *vnode;
  struct stat stat;

  Info ("SysFStat");

  filp = GetFilp(fd);

  if (filp == NULL) {
    return -EINVAL;
  }

  vnode = filp->vnode;

  vnode_lock(vnode);
  
  // FIXME: Should have vnodehold  when we know the vnode
  // But we don't acquire it , so no need for hold/put here.

  // If necessary, do getvattrs

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

  if (_stat == NULL) {
    return -EFAULT;
  }
  
  if (CopyOut(_stat, &stat, sizeof stat) != 0) {
    return -EFAULT;
  }
  
  return 0;
}


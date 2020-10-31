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

//#define KDEBUG 1

#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <sys/mount.h>

int Stat(char *_path, struct stat *_stat) {
  struct stat stat;
  struct Lookup lookup;
  int sc;

  if ((sc = Lookup(_path, 0, &lookup)) != 0) {
    return sc;
  }

  stat.st_dev = lookup.vnode->superblock->dev;
  stat.st_ino = lookup.vnode->inode_nr;
  stat.st_mode = lookup.vnode->mode;
  stat.st_nlink = lookup.vnode->nlink;
  stat.st_uid = lookup.vnode->uid;
  stat.st_gid = lookup.vnode->gid;
  stat.st_rdev = lookup.vnode->rdev;
  stat.st_size = lookup.vnode->size;
  stat.st_atime = lookup.vnode->atime;
  stat.st_mtime = lookup.vnode->mtime;
  stat.st_ctime = lookup.vnode->ctime;
  stat.st_blocks = lookup.vnode->blocks;
  stat.st_blksize = lookup.vnode->blksize;
  VNodePut(lookup.vnode);

  if (stat == NULL) {
    return -EFAULT;
  }
  
  return CopyOut(_stat, &stat, sizeof stat);
}

/*
 *
 */
int FStat(int fd, struct stat *_stat) {
  struct Filp *filp;
  struct VNode *vnode;
  struct stat stat;

  filp = GetFilp(fd);

  if (filp == NULL) {
    return -EINVAL;
  }

  vnode = filp->vnode;

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

  if (stat == NULL) {
    return -EFAULT;
  }
  
  return CopyOut(_stat, &stat, sizeof stat);
}


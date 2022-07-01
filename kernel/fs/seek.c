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

/*
 * FIXME: Limit block seeks to stat.block size multiples?
 *
 * vnode lock not needed
 * filp will need to either atomically set seek position or need locking if we remove BKL
 */
SYSCALL off_t sys_lseek(int fd, off_t pos, int whence) {
  struct Filp *filp;
  struct VNode *vnode;

  filp = get_filp(fd);

  if (filp == NULL) {
    return -EINVAL;
  }

  vnode = filp->vnode;

  if (!S_ISREG(vnode->mode) && !S_ISBLK(vnode->mode)) {
    return -EINVAL;
  } else if (whence == SEEK_SET) {
    filp->offset = pos;
  } else if (whence == SEEK_CUR) {
    filp->offset += pos;
  } else if (whence == SEEK_END) {
    filp->offset = vnode->size + pos;
  } else {
    return -EINVAL;
  }

  return (off_t)filp->offset;
}


/*
 *
 */
SYSCALL int sys_lseek64(int fd, off64_t *_pos, int whence) {
  struct Filp *filp;
  struct VNode *vnode;
  off64_t pos;
  int sc;
  
  pos = 0;

  sc = CopyIn(&pos, _pos, sizeof pos);


  Info ("lseek64, fd=%d, offs = %08x %08x, wh:%d", fd, (uint32_t)(pos >> 32), (uint32_t)pos, whence);
  Info ("CopyIn sc = %d", sc);

  filp = get_filp(fd);

  if (filp == NULL) {
    Info ("lseek64, not valid fd filp");
    return -EINVAL;
  }

  vnode = filp->vnode;

  if (!S_ISREG(vnode->mode) && !S_ISBLK(vnode->mode)) {
    Info ("lseek64 failed, not reg or blk");
    return -EINVAL;
  } else if (whence == SEEK_SET) {
    filp->offset = pos;
  } else if (whence == SEEK_CUR) {
    filp->offset += pos;
  } else if (whence == SEEK_END) {
    filp->offset = vnode->size + pos;
  } else {
    Info ("lseek64 failed");
    return -EINVAL;
  }

  pos = filp->offset;

  CopyOut(_pos, &pos, sizeof pos);

  Info ("lseek64 new offs = %08x %08x", (uint32_t)(pos>>32), (uint32_t)pos);
  return 0;
}


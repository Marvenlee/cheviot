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


/* @brief   Seek to a new file position
 * @param   fd,
 * @param   pos,
 * @param   whence,
 * @return  new seek position or negative errno on error
 *
 * FIXME: Limit block seeks to stat.block size multiples?
 * filp will need to either atomically set seek position or need locking if we remove BKL
 */
off_t sys_lseek(int fd, off_t pos, int whence) {
  struct Filp *filp;
  struct VNode *vnode;
  struct Process *current;

  Info("sys_lseek");
  
  current = get_current_process();
  filp = get_filp(current, fd);
  vnode = get_fd_vnode(current, fd);

  if (vnode == NULL) {
    return -EINVAL;
  }

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


/* @brief   Seek to a new file position
 * @param   fd,
 * @param   pos,
 * @param   whence,
 * @return  0 on success or negative errno on error
 */
int sys_lseek64(int fd, off64_t *_pos, int whence) {
  struct Process *current;
  struct Filp *filp;
  struct VNode *vnode;
  off64_t pos;
  int sc;

  Info("sys_lseek64");
  
  pos = 0;

  sc = CopyIn(&pos, _pos, sizeof pos);
  
  current = get_current_process();
  filp = get_filp(current, fd);
  vnode = get_fd_vnode(current, fd);

  if (vnode == NULL) {
    return -EINVAL;
  }

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

  pos = filp->offset;

  CopyOut(_pos, &pos, sizeof pos);
  return 0;
}


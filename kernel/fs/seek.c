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

#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/vm.h>

/*
 * FIXME: Limit block seeks to stat.block size multiples?
 */
off_t Seek(int fd, off_t pos, int whence) {
  struct Filp *filp;
  struct VNode *vnode;

  filp = GetFilp(fd);

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
int Seek64(int fd, off64_t *_pos, int whence) {
  struct Filp *filp;
  struct VNode *vnode;
  off64_t pos;

  CopyIn(&pos, _pos, sizeof pos);

  filp = GetFilp(fd);

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

  pos = filp->offset;

  CopyOut(_pos, &pos, sizeof pos);
  return 0;
}

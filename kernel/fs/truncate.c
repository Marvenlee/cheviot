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
#include <kernel/types.h>
#include <kernel/vm.h>
#include <poll.h>
#include <string.h>

/*
 *
 */
SYSCALL int sys_truncate(int fd, size_t sz) {
  struct Filp *filp = NULL;
  struct VNode *vnode = NULL;
  int err = 0;

  filp = GetFilp(fd);

  if (filp == NULL) {
    return -EINVAL;
  }

  vnode = filp->vnode;

  vnode_lock(vnode);

  if (!S_ISREG(vnode->mode)) {
    err = -EINVAL;
    goto exit;
  }

  if ((err = vfs_truncate(vnode, 0)) != 0) {
    goto exit;
  }

  wakeup_polls(vnode, POLLPRI, POLLPRI);
  vnode_unlock(vnode);
  return 0;

exit:
  vnode_unlock(vnode);
  return err;
}





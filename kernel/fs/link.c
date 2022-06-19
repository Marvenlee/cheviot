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
#include <poll.h>

/*
 *
 */
SYSCALL int sys_unlink(char *_path) {
  struct VNode *vnode = NULL;
  struct VNode *dvnode = NULL;
  int err = 0;
  struct Lookup lookup;

  Info ("Unlink");

  if ((err = Lookup(_path, LOOKUP_REMOVE, &lookup)) != 0) {
    return err;
  }

  vnode = lookup.vnode;
  dvnode = lookup.parent;

/* TODO: Could be anything other than dir
  need to check if it is a mount too.
*/

  if (!S_ISREG(vnode->mode)) {
    err = -EINVAL;  // FIXME: possibly ENOTFILE, EISDIR  ??
    goto exit;
  }
  
  vfs_unlink(dvnode, lookup.last_component);
  vnode->nlink --;
   
  WakeupPolls(vnode, POLLPRI, POLLPRI);
  
  // Need to do a cache invalidate in VNodePut 
  VNodePut(vnode);
  VNodePut(dvnode);
  return 0;
  
exit:
  VNodePut(vnode);
  VNodePut(dvnode);
  return err;
}





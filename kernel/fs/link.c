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
int sys_unlink(char *_path)
{
  struct VNode *vnode = NULL;
  struct VNode *dvnode = NULL;
  int err = 0;
  struct lookupdata ld;

  if ((err = lookup(_path, LOOKUP_REMOVE, &ld)) != 0) {
    return err;
  }

  vnode = ld.vnode;
  dvnode = ld.parent;

  /* TODO: Could be anything other than dir
   * need to check if it is a mount too.
   */

  if (!S_ISREG(vnode->mode)) {
    err = -EINVAL;  // FIXME: possibly ENOTFILE, EISDIR  ??
    goto exit;
  }
  
  // FIXME: Check vnode reference count and nlink count before
  // calling vfs_unlink() to remove file. 

  // TODO: If nlink is 0 and file is deleted send knote NOTE_DELETE
  // Need to set a flag to delete the file on last file handle close
  // if nlink is 0.  

  vnode->nlink --;
  vfs_unlink(dvnode, ld.last_component);  
   
  knote(&vnode->knote_list, NOTE_LINK);

  // TODO: Need to invalidate vnode if file is deleted (not vnode_put)

  vnode_put(vnode);
  vnode_put(dvnode);
  return 0;
  
exit:
  vnode_put(vnode);
  vnode_put(dvnode);
  return err;
}





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

/*
 *
 */
SYSCALL int sys_rename(char *oldpath, char *newpath) {
/*

  struct Lookup oldl;
  struct Lookup newl;
  int err;
  
  if ((err = lookup(oldpath, LOOKUP_REMOVE, &oldl)) != 0) {
    return err;
  }

  if ((err = lookup(newpath, LOOKUP_CREATE, &newl)) != 0) {
      VNodeRelease(oldl.vnode);
   VNodeRelease(oldl.dvnode);
    return err;
  }

    if (newl.dvnode->superblock != oldl.vnode->superblock) {
      VNodeRelease(oldl.vnode);
      VNodeRelease(oldl.dvnode);
      return -EINVAL;
    }

  //  dvnode = oldl.dvnode;

    if (S_ISDIR(oldl.vnode->mode)) {
      parent = newl.dvnode;

      while ((parent->vnode_covered == NULL) == 0) {
        grandparent = Lookup (parent, "..");
        VNodeRelease(parent);

        parent = grandparent;

        if (parent == oldlpath.vnode) {
        }
      }


    }


    // Ensure new path is not inside old path.
    // Get directory of newl, keep ascending up tree, until reaching mount point
    // If reaching oldl then cannot rename.


  */

  return 0;
}


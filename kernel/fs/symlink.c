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

#define KDEBUG 1

#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/proc.h>
#include <kernel/types.h>

/*
 *
 */
int SymLink(char *_path, char *_link) {
  struct Lookup lookup;
  int status;

  if ((status = Lookup(_path, LOOKUP_PARENT, &lookup)) != 0) {
    return status;
  }

  if (lookup.vnode != NULL) {
    VNodePut(lookup.parent);
    VNodePut(lookup.vnode);
    return -EEXIST;
  }

  //    status = vfs_mklink(lookup.parent, lookup.last_component, _link);
  VNodePut(lookup.parent);
  return status;
}

/*
 *
 */
int ReadLink(char *_path, char *_link, size_t link_size) {
  struct Lookup lookup;
  int status;
  
  if ((status = Lookup(_path, 0, &lookup)) != 0) {
    return status;
  }

  if (lookup.vnode == NULL) {
    return -EEXIST;
  }

  // TODO, check if vnode is a symlink.
  if (!S_ISLNK(lookup.vnode->mode)) {
    VNodePut(lookup.vnode);
    return -ENOLINK;
  }

  //    status = vfs_readlink(lookup.vnode, _link, link_size);
  VNodePut(lookup.vnode);
  return status;
}

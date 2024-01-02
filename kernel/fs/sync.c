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


/* @brief   Write all mounted filesystems to disk
 *
 * Send sync message to all mounted filesystems
 */
int sys_sync(void)
{
/*
  struct SuperBlock *sb;
  
  sb = LIST_HEAD(&mounted_superblock_list);
  
  while (sb != NULL) {
    sc = vfs_syncfs(sb);
  
    sb = LIST_NEXT(sb, mounted_superblock_link);
  }
*/    
  return -ENOSYS;
}


/* @brief   Write all unwritten blocks of a file to disk
 *
 */
int sys_fsync(int fd)
{ 
  struct Filp *filp;
  struct VNode *vnode;
  ssize_t xfered;
  struct Process *current;
  int sc;
  
  current = get_current_process();
  filp = get_filp(current, fd);
  vnode = get_fd_vnode(current, fd);

  if (vnode == NULL) {
    return -EINVAL;
  }

  if (is_allowed(vnode, W_OK) != 0) {
    return -EACCES;
  }
  
  vnode_lock(vnode);
  sc = vfs_fsync(vnode);
  vnode_unlock(vnode);
  
  return sc;  
}




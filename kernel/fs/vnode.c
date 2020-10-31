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
#include <string.h>

void ValidateVNode(struct VNode *vnode)
{
  if ((vm_addr)vnode < (vm_addr)vnode_table || (vm_addr)vnode >= (vm_addr)(vnode_table + max_vnode)) {
    Info ("vnode out of range, addr:%08x", (vm_addr)vnode);
    KernelPanic();
  }
}

struct VNode *VNodeNew(struct SuperBlock *sb, int inode_nr) {
  struct VNode *vnode;

//  KLog ("VNodeNew sb = %08x, ino = %d", (vm_addr)sb, inode_nr);

  vnode = LIST_HEAD(&vnode_free_list);

  if (vnode == NULL) {
    Warn("No free VNodes");
    return NULL;
  }

  LIST_REM_HEAD(&vnode_free_list, vnode_entry);

  // Remove existing vnode from DNLC and file cache
  // DNamePurgeVNode(vnode);
  // BSync(vnode)
  //
  ValidateVNode(vnode);

  memset(vnode, 0, sizeof *vnode);
// FIXME vnode->vnode_mounted_here = NULL;
//  vnode->vnode_covered = NULL;

// TODO: Replace with rendez/rw direction flag (third state = write pending) /read lock count/
//  vnode->busy = 1;
  vnode->reader_cnt = 0;
  vnode->writer_cnt = 0;
  
  vnode->flags = 0;
  vnode->superblock = sb;
  vnode->inode_nr = inode_nr;
  vnode->reference_cnt = 1;
  sb->reference_cnt++;
  
  LIST_INIT(&vnode->poll_list);
  LIST_INIT(&vnode->vnode_list);
  LIST_INIT(&vnode->directory_list);
  
  InitRendez(&vnode->rendez);
  
  Info ("vnode_new (ino_nr = %d, sb = %08x)", vnode->inode_nr, (vm_addr)sb);
  return vnode;
}

/*
 * Finds a vnode from a given superblock and vnode number.
 * Returns an existing vnode if it is still in the vnode cache.
 * If the vnode is not in the cache then another free vnode is
 * chosen. Data pointed to by the vnode->data field is freed first
 * before returning the vnode.
 *
 * returns a vnode structure, if not existing already then V_VALID
 * is cleared in the flags field.
 */

struct VNode *VNodeGet(struct SuperBlock *sb, int inode_nr) {
  struct VNode *vnode;

  KLog ("VNodeGet sb = %08x, ino = %d", (vm_addr)sb, inode_nr);

  while (1) {
    if (sb->flags & S_ABORT)
      return NULL;

    if ((vnode = FindVNode(sb, inode_nr)) != NULL) {
/*
      if (vnode->busy == 1) {
        KLog ("Found vnode, vnode busy, sleeping");
        TaskSleep(&vnode->rendez);
        continue;
      }
*/
      Info ("VNodeGet found vnode: %08x, inode_nr = %d", (vm_addr)vnode, vnode->inode_nr);


      if ((vnode->flags & V_FREE) == V_FREE) {
        LIST_REM_ENTRY(&vnode_free_list, vnode, vnode_entry);
      }

//      vnode->busy = 1;


      vnode->reference_cnt++;
      sb->reference_cnt++;

      return vnode;
    } else {
      return NULL;
    }
  }
}

/*
 * Used to increment reference count of existing VNode.  Used within FChDir so
 * that proc->current_dir counts as reference.
 */
void VNodeIncRef(struct VNode *vnode) {
  vnode->reference_cnt++;
  vnode->superblock->reference_cnt++;
}

void VNodeDecRef(struct VNode *vnode) {
  vnode->reference_cnt--;
  vnode->superblock->reference_cnt--;
}


// FIXME:  Needed? Used to destroy anonymous vnodes such as pipes/queues

void VNodeFree(struct VNode *vnode) {

  if (vnode == NULL) {
    return;
  }

  vnode->flags = V_FREE;
  LIST_ADD_HEAD(&vnode_free_list, vnode, vnode_entry);

  vnode->busy = 0;
  vnode->reference_cnt = 0;
  TaskWakeupAll(&vnode->rendez);
}

/*
 * Return VNode to cached pool.
 */
void VNodePut(struct VNode *vnode) {

  if (vnode == NULL) {
    Warn("VNodePut null");
    return;
  }

  vnode->reference_cnt--;
  
  if (vnode->superblock != NULL) {  
    vnode->superblock->reference_cnt--;
  } else {
    Warn("vnode->superblock null");
  }
  
  if (vnode->reference_cnt == 0) {

    
    // FIXME: IF vnode->link_count == 0,  delete entries in cache.
 /*
    
    if (vnode->nlink == 0) {
      
      // Thought we removed vnode->vfs ??????????
      vnode->vfs->remove(vnode);
    }  
*/    

    if ((vnode->flags & V_ROOT) == 0) {
      vnode->flags |= V_FREE;
      LIST_ADD_TAIL(&vnode_free_list, vnode, vnode_entry);
    }
    
    vnode->superblock = NULL;
    vnode->flags = 0;

    // FIXME: may want to reinstate: decrement sb ref count 

  }

  vnode->busy = 0;
  TaskWakeupAll(&vnode->rendez);
}

/*
 * FindVNode();
 * FIXME: TODO : Hash vnode by sb and inode_nr
 */

struct VNode *FindVNode(struct SuperBlock *sb, int inode_nr) {
  int v;

  for (v = 0; v < NR_VNODE; v++) {
    if (/* FIXME:????? (vnode_table[v].flags & V_FREE) != V_FREE &&*/
        vnode_table[v].superblock == sb &&
        vnode_table[v].inode_nr == inode_nr) {
      return &vnode_table[v];
    }
  }
  return NULL;
}








void VNodeLock(struct VNode *vnode) {
  while (vnode->busy == 1) {
    TaskSleep(&vnode->rendez);
  }

  vnode->busy = 1;
}


// TODO : Vnode Shared lock
void VNodeLockShared(struct VNode *vnode) {
  while (vnode->busy == 1) {
    TaskSleep(&vnode->rendez);
  }

  vnode->busy = 1;
}


void VNodeUnlock(struct VNode *vnode) {
  vnode->busy = 0;
  TaskWakeupAll(&vnode->rendez);
}

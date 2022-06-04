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

// #define KDEBUG

#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <string.h>


// Static prototypes
static struct VNode *VNodeFind(struct SuperBlock *sb, int inode_nr);


/* @brief Allocate a new vnode
 *
 */
struct VNode *VNodeNew(struct SuperBlock *sb, int inode_nr) {
  struct VNode *vnode;

  Info ("VNodeNew: %d", inode_nr);

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

  // Make sure vnode doesn't already exist
  
  

  memset(vnode, 0, sizeof *vnode);
  
  vnode->busy = true;
  
  vnode->vnode_mounted_here = NULL;
  vnode->vnode_covered = NULL;

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
  
  Info ("VNodeNew (ino_nr = %d, sb = %08x)", vnode->inode_nr, (vm_addr)sb);
  return vnode;
}

/* @brief Find an existing vnode
 *
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

  while (1) {
    if (sb->flags & S_ABORT)
      return NULL;

    if ((vnode = VNodeFind(sb, inode_nr)) != NULL) {

      // Use VNodeLock outside of this.
/*      
      if (vnode->busy == true) {
        Info ("******** Found vnode, vnode busy, sleeping **********");
        TaskSleep(&vnode->rendez);
        continue;
      }
*/
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

// FIXME:  Needed? Used to destroy anonymous vnodes such as pipes/queues
// Will be needed if VFS is unmounted to remove all vnodes belonging to VFS

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
 * @brief Release a VNode
 *
 * VNode is returned to the cached pool where it can lazily be freed.
 */
void VNodePut(struct VNode *vnode) {
  KASSERT(vnode != NULL);
  KASSERT(vnode->superblock != NULL);
//  KASSERT(vnode->busy == true);     // This should be locked prior to putting
  
  
  vnode->reference_cnt--;  
  vnode->superblock->reference_cnt--;
  
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
  }
  else
  {
    // Do we mark it as false
    vnode->busy = false;
  }
  
  TaskWakeupAll(&vnode->rendez);
}

/* @brief Acquire exclusive access to a vnode
 * 
 */
void VNodeLock(struct VNode *vnode) {
//  while (vnode->busy == 1) {
//    TaskSleep(&vnode->rendez);
//  }

  vnode->busy = 1;
}

/* @brief Relinquish exclusive access to a vnode
 *
 */
void VNodeUnlock(struct VNode *vnode) {
  vnode->busy = 0;
//  TaskWakeupAll(&vnode->rendez);
}


/* @brief: Find an existing vnode in the vnode cache
 * FIXME: TODO : Hash vnode by sb and inode_nr
 */

static struct VNode *VNodeFind(struct SuperBlock *sb, int inode_nr) {
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



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
#include <kernel/kqueue.h>


// Static prototypes
static struct VNode *vnode_find(struct SuperBlock *sb, int inode_nr);


/* @brief   Lookup a vnode from a file descriptor
 *
 * @param   proc, process in which the file descriptor belongs
 * @param   fd, file descriptor to find vnodce of
 * @return  vnode if found, otherwise returns NULL
 */
struct VNode *get_fd_vnode(struct Process *proc, int fd)
{
  struct Filp *filp;

  if (fd < 0 || fd >= OPEN_MAX) {
    return NULL;
  }
  
  filp = proc->fproc->fd_table[fd];
    
  if (filp == NULL) {
    return NULL;
  }
  
  if (filp->type != FILP_TYPE_VNODE) {
    return NULL;
  }
  
  return filp->u.vnode;
}


/* @brief   Close a file descriptor reference to a open file
 *
 * @param   proc, process in which the file descriptor belongs
 * @param   fd, file descriptor of file to close
 * @return  0 on success, negative errno on failure
 *
 * FIXME: on vnode_put, we need to send a vfs_close() on zero reference_count
 *        Same here?
 */
int close_vnode(struct Process *proc, int fd)
{   
  struct Filp *filp;
  struct Pipe *pipe;
  struct VNode *vnode;

  filp = get_filp(proc, fd);
  vnode = get_fd_vnode(proc, fd);
  
  if (vnode == NULL) {
    return -EINVAL;  
  }
  
  vnode_lock(vnode);
  
  
  
  
  if (vnode->reference_cnt == 1) {
    // Do any special cleanup of file, dir, fifo ?
  }

  vnode_put(vnode);
  
  // TODO: decrement vnode reference count, if 0 free vnode.
  free_fd_filp(proc, fd);
  
  return 0;
}


/* @brief   Allocate a new vnode
 *
 * Allocate a new vnode object, assign an inode_nr to it and lock it.
 * A call to vnode_find() should be called prior to this to see if the vnode
 * already exists.
 */
struct VNode *vnode_new(struct SuperBlock *sb, int inode_nr)
{
  struct VNode *vnode;

  vnode = LIST_HEAD(&vnode_free_list);

  if (vnode == NULL) {
    return NULL;
  }

  LIST_REM_HEAD(&vnode_free_list, vnode_entry);

  // Remove existing vnode from DNLC and file cache
  // DNamePurgeVNode(vnode);
  // BSync(vnode)
  // TODO: Need to handle case if vnode is not in cache and no free slot availble,
  // release any existing vnode (remember to flush buf cache of file) and reuse it.    

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
  
  LIST_INIT(&vnode->vnode_list);
  LIST_INIT(&vnode->directory_list);
  LIST_INIT(&vnode->knote_list);
  
  InitRendez(&vnode->rendez);
  
  Info ("vnode_new = %08x", (uint32_t)vnode);
  
  return vnode;
}


/* @brief   Find an existing vnode
 */
struct VNode *vnode_get(struct SuperBlock *sb, int inode_nr)
{
  struct VNode *vnode;

  Info ("vnode_get(sb:%08x, ino_nr:%d)",(uint32_t)sb, inode_nr);

  while (1) {                       // FIXME: Why is a while loop needed ?
    if (sb->flags & S_ABORT) {
      return NULL;
    }
    
    if ((vnode = vnode_find(sb, inode_nr)) != NULL) {
      vnode->reference_cnt++;
      sb->reference_cnt++;
    
      while (vnode->busy) {
        Info("vnode %08x busy, sleeping", (uint32_t)vnode);
        TaskSleep(&vnode->rendez);
      }

      vnode->busy = true;

      if ((vnode->flags & V_FREE) == V_FREE) {
        LIST_REM_ENTRY(&vnode_free_list, vnode, vnode_entry);
      }

      Info ("vnode_get retval: %08x", (uint32_t)vnode);
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
void vnode_inc_ref(struct VNode *vnode)
{
  vnode->reference_cnt++;
  vnode->superblock->reference_cnt++;
}

/*
 * @brief   Release a VNode
 *
 * VNode is returned to the cached pool where it can lazily be freed.
 * 
 * FIXME: on vnode_put, we need to send a vfs_close() on zero reference_count
 */
void vnode_put(struct VNode *vnode)
{
  KASSERT(vnode != NULL);
  KASSERT(vnode->superblock != NULL);
  // KASSERT(vnode->busy == true);     // Fails if vnode and parent are same path = "/." then most ops do 2 VNodePuts on same vnode.
  
  vnode->busy = false;
    
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
  
  TaskWakeupAll(&vnode->rendez);
}

/* 
 * FIXME:  Needed? Used to destroy anonymous vnodes such as pipes/queues
 * Will be needed if VFS is unmounted to remove all vnodes belonging to VFS
 */
void vnode_free(struct VNode *vnode)
{
  if (vnode == NULL) {
    return;
  }

  vnode->flags = V_FREE;
  LIST_ADD_HEAD(&vnode_free_list, vnode, vnode_entry);

  vnode->busy = false;
  vnode->reference_cnt = 0;
  TaskWakeupAll(&vnode->rendez);
}


/* @brief   Acquire exclusive access to a vnode
 * 
 */
void vnode_lock(struct VNode *vnode)
{
  KASSERT(vnode != NULL);
  
  while (vnode->busy == true) {
    TaskSleep(&vnode->rendez);
  }

  vnode->busy = true;
}


/* @brief   Relinquish exclusive access to a vnode
 *
 */
void vnode_unlock(struct VNode *vnode)
{
  KASSERT(vnode != NULL);
  KASSERT(vnode->busy == true);
  
  vnode->busy = false;
  TaskWakeupAll(&vnode->rendez);
}


/* @brief   Find an existing vnode in the vnode cache
 *
 * TODO : Hash vnode by sb and inode_nr
 */
static struct VNode *vnode_find(struct SuperBlock *sb, int inode_nr)
{
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



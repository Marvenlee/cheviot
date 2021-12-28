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


/*
 * ReadFromCache();
 */
ssize_t ReadFromCache (struct VNode *vnode, void *dst, size_t sz, off64_t *offset) {
  struct Buf *buf;
  off_t cluster_base;
  off_t cluster_offset;
  size_t nbytes_xfered;
  size_t nbytes_remaining;
  struct Process *current;
  
  current = GetCurrentProcess();

  if (*offset >= vnode->size) {
    return 0;
  }

  nbytes_remaining =
      (vnode->size - *offset < sz) ? vnode->size - *offset : sz;

  while (nbytes_remaining > 0) {
    cluster_base = ALIGN_DOWN(*offset, CLUSTER_SZ);
    cluster_offset = *offset % CLUSTER_SZ;

    nbytes_xfered = (CLUSTER_SZ - cluster_offset < nbytes_remaining)
                        ? CLUSTER_SZ - cluster_offset
                        : nbytes_remaining;

    // Need locks for buffer (only single buffer of given offset, additional requests
    // must block on buffer->busy.
    
    buf = BRead(vnode, cluster_base);

    if (buf == NULL) {
      // update datestamps still ?
      return sz - nbytes_remaining;
    }

    CopyOut(dst, buf->addr + cluster_offset, nbytes_xfered);

    BRelse(buf);

    dst += nbytes_xfered;
    *offset += nbytes_xfered;
    nbytes_remaining -= nbytes_xfered;
  }

  VNodeLock(vnode);
  // Update access datestamps.      
  VNodeUnlock(vnode);

  return sz - nbytes_remaining;
}


/*
 * WriteToCache();
 */
ssize_t WriteToCache (struct VNode *vnode, void *src, size_t sz, off64_t *offset) {
  struct Buf *buf;
  off_t cluster_base;
  off_t cluster_offset;
  size_t nbytes_xfered;
  size_t nbytes_remaining;
  struct Process *current;
  
  current = GetCurrentProcess();

  nbytes_remaining = sz;

  while (nbytes_remaining > 0) {
    cluster_base = ALIGN_DOWN(*offset, CLUSTER_SZ);
    cluster_offset = *offset % CLUSTER_SZ;

    nbytes_xfered = (CLUSTER_SZ - cluster_offset < nbytes_remaining)
                        ? CLUSTER_SZ - cluster_offset
                        : nbytes_remaining;

    buf = BRead(vnode, cluster_base);

    if (buf == NULL) {
      // update datestamps still?
      return sz - nbytes_remaining;
    }

    if (buf->cluster_size < cluster_offset + nbytes_xfered) {
      BResize(buf, cluster_offset + nbytes_xfered);
    }

    CopyIn(buf->addr + cluster_offset, src, nbytes_xfered);    

    src += nbytes_xfered;
    *offset += nbytes_xfered;
    nbytes_remaining -= nbytes_xfered;

    if (*offset > vnode->size) {
      vnode->size = *offset;
    }

    BAWrite(buf); 
    // Take offset+sz params, so w know which FS blocks were changed.
              // Or bitmap of 1k blocks?
  }

  VNodeLock(vnode);
  // Update access datestamps.      
  VNodeUnlock(vnode);


  return sz - nbytes_remaining;
}


/*
 * GetBlk();
 */
struct Buf *GetBlk(struct VNode *vnode, uint64_t cluster_offset) {
  struct Buf *buf;
  struct Pageframe *pf;
  vm_addr pa;
  size_t cluster_size;

  //	Log ("GetBlk(%d)", (int)cluster_offset);

  while (1) {
    if ((buf = FindBlk(vnode, cluster_offset)) != NULL) {
      if (buf->flags & B_BUSY) {
        TaskSleep(&buf->rendez);
        continue;
      }

      buf->flags |= B_BUSY;
      LIST_REM_ENTRY(&buf_avail_list, buf, free_link);

      return buf;
    } else {
      if ((buf = LIST_HEAD(&buf_avail_list)) == NULL) {
        TaskSleep(&buf_list_rendez);
        continue;
      }

      LIST_REM_HEAD(&buf_avail_list, free_link);
      buf->flags |= B_BUSY;

      if (buf->flags & B_DELWRI) {
        // For the moment, write entire cluster
        //				buf->offset = 0;
        //				buf->remaining = buf->cluster_size;
        buf->flags = (buf->flags | B_WRITE | B_WRITECLUSTER | B_ASYNC) &
                     ~(B_READ | B_DELWRI);
        Strategy(buf);
        continue;
      }

      if (buf->flags & B_VALID) {
        LIST_REM_ENTRY(&buf_hash[buf->cluster_offset % BUF_HASH], buf,
                       lookup_link);

        for (int t = 0; t < CLUSTER_SZ / PAGE_SIZE; t++) {
          PmapCacheExtract((vm_addr)buf->addr + t * PAGE_SIZE, &pa);
          pf = PmapPaToPf(pa);
          PmapCacheRemove((vm_addr)buf->addr + t * PAGE_SIZE);
          FreePageframe(pf);
        }
      }

      if (S_ISDIR(vnode->mode)) {
        cluster_size = CLUSTER_SZ;
      } else if (cluster_offset >= vnode->size) {
        cluster_size = 0;
      } else if (vnode->size - cluster_offset < CLUSTER_SZ) {
        cluster_size = ALIGN_UP(vnode->size - cluster_offset, PAGE_SIZE);
      } else {
        cluster_size = CLUSTER_SZ;
      }

      for (int t = 0; t < cluster_size / PAGE_SIZE; t++) {
        pf = AllocPageframe(PAGE_SIZE);
        PmapCacheEnter((vm_addr)buf->addr + t * PAGE_SIZE, pf->physical_addr);
      }

      PmapFlushTLBs();
      buf->cluster_size = cluster_size;
      buf->flags &= ~B_VALID;
      buf->vnode = vnode;

      buf->cluster_offset = cluster_offset;
      LIST_ADD_HEAD(&buf_hash[buf->cluster_offset % BUF_HASH], buf,
                    lookup_link);
      //		Info ("GetBlk setting cluster_offset = %d",
      // cluster_offset);

      return buf;
    }
  }
}

int BResize(struct Buf *buf, size_t sz) {
  struct Pageframe *pf;
  vm_addr pa;

  sz = ALIGN_UP(sz, PAGE_SIZE);

  if (sz > buf->cluster_size) {
    for (int t = buf->cluster_size / PAGE_SIZE; t < sz / PAGE_SIZE; t++) {
      pf = AllocPageframe(PAGE_SIZE);
      PmapCacheEnter((vm_addr)buf->addr + t * PAGE_SIZE, pf->physical_addr);
    }
  } else if (sz < buf->cluster_size) {
    for (int t = sz / PAGE_SIZE; t < buf->cluster_size / PAGE_SIZE; t++) {
      PmapCacheExtract((vm_addr)buf->addr + t * PAGE_SIZE, &pa);
      pf = PmapPaToPf(pa);
      PmapCacheRemove((vm_addr)buf->addr + t * PAGE_SIZE);
      FreePageframe(pf);
    }
  }

  PmapFlushTLBs();
  buf->cluster_size = sz;
  return 0;
}


/*
 * FindBlk();
 */
struct Buf *FindBlk(struct VNode *vnode, uint64_t cluster_offset) {
  struct Buf *buf;

  // TODO: Calculate hash based on superblock, vnode_nr and cluster_offset;

  buf = LIST_HEAD(&buf_hash[cluster_offset % BUF_HASH]);

  while (buf != NULL) {
    if (buf->vnode == vnode && buf->cluster_offset == cluster_offset)
      return buf;

    buf = LIST_NEXT(buf, lookup_link);
  }

  return NULL;
}


/*
 * BRead();
 *
 * Searches for the cluster in the clusterfer cache, if not present allocates
 * a cluster and reads the data from disk.
 */
struct Buf *BRead(struct VNode *vnode, uint64_t cluster_offset) {
  struct Buf *buf;

  Info ("BRead");
  buf = GetBlk(vnode, cluster_offset);

  if (buf->flags & B_VALID) {
    //        Info ("BRead(%d) in cache, returning", (int)cluster_offset);

    return buf;
  }

  //    Info ("BRead(%d), strategy", (int)cluster_offset);

  buf->flags = (buf->flags | B_READ) & ~(B_WRITE | B_ASYNC);
  Strategy(buf);

  while (!(buf->flags & B_IODONE)) {
    TaskSleep(&buf->rendez);
  }
  
  buf->flags &= ~B_IODONE;

  if (buf->flags & B_ERROR) {
    BRelse(buf);
    return NULL;
  }

  buf->flags = (buf->flags | B_VALID) & ~B_READ;
  //	buf->offset = 0;
  //	buf->remaining = cluster_size;
  return buf;
}


/*
 * BRelse();
 *
 * Releases a cluster, wakes up next coroutine waiting for the cluster
 */
void BRelse(struct Buf *buf) {
  if (buf->flags & (B_ERROR | B_DISCARD)) {
    LIST_REM_ENTRY(&buf_hash[buf->cluster_offset % BUF_HASH], buf, lookup_link);
    buf->flags &= ~(B_VALID | B_ERROR);
    buf->cluster_offset = -1;
    buf->vnode = NULL;

    if (buf->addr != NULL) {
      LIST_ADD_HEAD(&buf_avail_list, buf, free_link);
    }
  } else if (buf->addr != NULL) {
    LIST_ADD_TAIL(&buf_avail_list, buf, free_link);
  }

  buf->flags &= ~B_BUSY;
  TaskWakeupAll(&buf_list_rendez);
  TaskWakeupAll(&buf->rendez);
}


/*
 * BDWrite();
 *
 * How do we do this,  notify FS handler of each write, but only do actual write
 * if timer expired?
 * BUT That means every write requires context switching to FS Handler.  Would
 * prefer a delay if possible.
 */
int BDWrite(struct Buf *buf) {
  /*
        if (buf->flags & B_DELWRI && buf->expire_time.seconds <
   current_time.seconds)
        {
                BRelse (buf);
                return 0;
        }
*/

  buf->flags = (buf->flags | B_WRITE | B_DELWRI) & ~(B_READ | B_ASYNC);
  //	buf->expire_time.seconds = current_time.seconds + DELWRI_SECONDS;
  //	buf->expire_time.tv_usec = 0;
  BRelse(buf);
  return 0;
}

/*
 * BWrite();
 *
 * Writes a block to disk and releases the cluster.  Waits for IO to complete
 * before
 * returning to the caller.
 */

// FIXME:  Can we have a bitmap of pages to write?
int BWrite(struct Buf *buf) {
  buf->flags = (buf->flags | B_WRITE) & ~(B_READ | B_ASYNC);
  Strategy(buf);

  while (!(buf->flags & B_IODONE)) {
    TaskSleep(&buf->rendez);
  }
  
  buf->flags &= ~B_IODONE;

  if (buf->flags & B_ERROR) {
    BRelse(buf);
    return -1;
  }

  buf->flags &= ~B_WRITE;
  BRelse(buf);
  return 0;
}

// Sync all blocks belonging to a vnode
// Sync if a vnode is reused.

int BSync(struct VNode *vnode) {
  // Write file blocks to disk,  Unmap blocks, returning to free mem.

  return -1;
}

// Release all cache elements on a block device, optionally syncing
// Used for device eject or forced removal.
int BSyncFS(struct SuperBlock *sb, bool sync) { return -1; }

// Change BlockDev to partition?

size_t ShrinkCache(size_t free) {
  // Find out how much we need
  // while (cache_shrink_busy)
  // cond_wait();

  // Flush LRU until
  return 0;
}

/*
 * BAWrite();
 *
 * Write cluster asynchronously (um, shouldn't that be what BWrite does?
 * Release the cluster after finishing write asynchronously.
 * Perhaps put it to the FRONT of the LRU list
 *
 */

int BAWrite(struct Buf *buf) {
  buf->flags = (buf->flags | B_WRITE | B_ASYNC) & ~B_READ;
  Strategy(buf);
  return 0;
}

/*
 * Strategy();
 *
 * FIXME: Test for sb abort, return as if IOERROR IODONE occured.
 *
 * Do we need a separate task for Strategy???
 *
 * Depending on whether in-kernel FS, why not send message to fs-handler task.
 * Eventually it will call back with vnode_op_write(file, buf, sz) and/or
 * replymsg;
 * These will wake up what is currently waiting on Strategy task.
 *
 * Is there one strategy task?   Or instead of strategy task we put message to
 * FS handler message port?  (or call into handler functions for non-fs modules)
 *
 * Have some cache inteligence, if streaming reads, expunge last 64k from cache.
 * Perhaps read-ahead option too,  Can we return early whilst we read some more
 * data?
 *
 */

void Strategy(struct Buf *buf) {
  struct VNode *vnode;
  off64_t offset;
  int sc;
  
  
  vnode = buf->vnode;
  offset = buf->cluster_offset;

  if (buf->flags & B_DELWRI) {
//    buf->expiration_time = now + 5 seconds;
    LIST_ADD_TAIL(&delayed_write_queue, buf, delayed_write_link);
  } else {

      if (buf->flags & B_READ) {
        sc = vfs_read(vnode, buf->addr, buf->cluster_size, &offset);
      } else if (buf->flags & B_WRITE) {
        sc = vfs_write(vnode, buf->addr, buf->cluster_size, &offset);
      }

      buf->flags |= B_IODONE;     // FIXME: add b_valid ???

      if (sc < 0) {
        Error ("Strategy IO error %d", sc);
      }   
  }

  // Do we need to wakeup any rendez?
}


/*
 * Block Flush daemon, periodically wake up and add Buf's to each VNode's message queues
 */

void BDFlush(void) {
  struct Buf *buf;
  struct VNode *vnode;
  struct SuperBlock *sb;

  
  while (1) {
    
    // TODO:  why sleep?  Any other trigger for a flush?
    SysSleep(2);

    // Move into DoFSync().
    

    buf = LIST_HEAD(&delayed_write_queue);

    if (buf == NULL) {
      continue;
    }

    vnode = buf->vnode;

    if (buf->flags & B_BUSY) {
      TaskSleep(&buf->rendez);
    }

    buf->flags |= B_BUSY;

    // Is this buffer still on the DELWRI list (not just head)?

    if (!(buf->flags & B_DELWRI)) {
      buf->flags &= ~B_BUSY;
      TaskWakeupAll(&buf->rendez);
      TaskWakeupAll(&vnode->rendez);
      continue;
    }

    LIST_REM_ENTRY(&delayed_write_queue, buf, delayed_write_link);
    buf->flags &= ~B_DELWRI;
    buf->flags |= B_WRITE;

// Need to queue on each message port, in order.
//    Is vfs read/write/delwrite different to sync messages?
//    Perhaps vfs_strategy(flags.....)
      // vfs_read and vfs_write for char and block devices, strategy for FS.      
//    vfs_delwrite(vnode, buf->addr, buf->cluster_size, &offset);

    buf->msg.iov = NULL;
    buf->msg.buf = buf;

    sb = vnode->superblock;
    
    KPutMsg(&sb->msgport, vnode, &buf->msg);


//  Again vfs_delwri should be non-blocking, the following should be done when
// driver replies to message.  (not here).
    
//    buf->flags &= ~B_BUSY;
//    vnode->busy = FALSE;

//    TaskWakeupAll(&buf->rendez);



  }
}





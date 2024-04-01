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
#include <kernel/vm.h>
#include <kernel/timer.h>
#include <kernel/types.h>
#include <string.h>


/* @brief   Read from a file through the VFS file cache
 *
 * @param   vnode, file to read from
 * @param   dst, destination address to copy file data to (kernel or user)
 * @param   sz, number of bytes to read
 * @param   inkernel, set to true if the destination address is in the kernel (for kread)
 * @return  number of bytes read or negative errno on failure  
 */
ssize_t read_from_cache(struct VNode *vnode, void *dst, size_t sz, off64_t *offset, bool inkernel)
{
  struct Buf *buf;
  off64_t cluster_base;
  off64_t cluster_offset;
  size_t nbytes_xfer;
  size_t nbytes_total;
  size_t remaining_in_file;
  size_t nbytes_to_read;
  size_t remaining_to_xfer;  
  size_t remaining_in_cluster;    
  struct Process *current;

  current = get_current_process();

  if (*offset >= vnode->size) {
    return 0;
  }

	if (*offset >= vnode->size) {
		return 0;
	}

	remaining_in_file = vnode->size - *offset;
	
	nbytes_total = 0;
  nbytes_to_read = (remaining_in_file < sz) ? remaining_in_file : sz;

  while (nbytes_total < nbytes_to_read) {  
    cluster_base = ALIGN_DOWN(*offset, CLUSTER_SZ);
    cluster_offset = *offset % CLUSTER_SZ;

		remaining_to_xfer = nbytes_to_read - nbytes_total;
		remaining_in_cluster = CLUSTER_SZ - cluster_offset;
		nbytes_xfer = (remaining_to_xfer < remaining_in_cluster) ? remaining_to_xfer : remaining_in_cluster;
		
    buf = bread(vnode, cluster_base);

    if (buf == NULL) {
      Warn("bread buf is NULL");
    	// TODO: Maybe return an IO error.
      break;
    }

    if (inkernel == true) {
        memcpy(dst, buf->data + cluster_offset, nbytes_xfer);
    } else {    
        if (CopyOut(dst, buf->data + cluster_offset, nbytes_xfer) != 0) {
        	break;
        }
    }
    
    brelse(buf);

    dst += nbytes_xfer;
    *offset += nbytes_xfer;
    nbytes_total += nbytes_xfer;
  }

//	Info("read_from_cache() total= %d", nbytes_total);
  return nbytes_total;
}


/* @brief   Write to a file through the VFS file cache
 *
 * @param   vnode, file to write to
 * @param   src, user-space source address of the data to be written to the file
 * @param   sz, number of bytes to write
 * @return  number of bytes written or negative errno on failure  
 *
 * If we are writing a full block, can we avoid reading it in?
 * if block doesn't exist, does bread create it?
 */
ssize_t write_to_cache(struct VNode *vnode, void *src, size_t sz, off64_t *offset)
{
  struct Buf *buf;
  off_t cluster_base;
  off_t cluster_offset;
  size_t nbytes_xfer;
  size_t nbytes_total;
  size_t nbytes_to_write;
  size_t remaining_to_xfer;  
  size_t remaining_in_cluster;  
  struct Process *current;
  
    
  current = get_current_process();

	nbytes_total = 0;
  nbytes_to_write = sz;

  while (nbytes_total < nbytes_to_write) {  
    cluster_base = ALIGN_DOWN(*offset, CLUSTER_SZ);
    cluster_offset = *offset % CLUSTER_SZ;

		remaining_to_xfer = nbytes_to_write - nbytes_total;
		remaining_in_cluster = CLUSTER_SZ - cluster_offset;
		nbytes_xfer = (remaining_to_xfer < remaining_in_cluster) ? remaining_to_xfer : remaining_in_cluster;
		
		if (cluster_base < vnode->size) {
      buf = bread(vnode, cluster_base);
    } else {
      buf = bread_zero(vnode, cluster_base);
    }
    
    if (buf == NULL) {
      Warn("bread buf is NULL");
    	// TODO: Maybe return an IO error.
      break;
    }

    CopyIn(buf->data + cluster_offset, src, nbytes_xfer);    
		 
    src += nbytes_xfer;
    *offset += nbytes_xfer;
    nbytes_total += nbytes_xfer;

    // Update file size if we have written past the end of file    
    if (*offset > vnode->size) {
      vnode->size = *offset;
    }

    if ((*offset % CLUSTER_SZ) == 0) {
      bawrite(buf);
    } else {
      bdwrite(buf);
    }    
  }

  return nbytes_total;
}


/* @Brief   Get a cached block
 *
 * @param   vnode, file to get cached block of
 * @param   cluster_offset, offset within file to read
 * @return  buf on success, NULL if it cannot find or create a buf
 *
 * This cache operates at the file level and not the block level. As such it
 * has no understanding of the on disk formatting of a file system. User space
 * servers implementing file system handlers are free to implement block level
 * caching if needed.
 *
 * See Maurice Bach's 'The Design of the UNIX Operating System' for notes on
 * getblk, bread, bwrite, bawrite and brelse operations (though they applied to
 * the block level in the book).
 *
 * If cache is now single page blocks instead of larger 16k or 64k clusters
 * can we use the phys mem mapping to access pages?
 * Do we need to map cached blocks into a certain area of the kernel?
 *
 * Do we need to write the entire cluster?  Is this handled by strategy params?
 */
struct Buf *getblk(struct VNode *vnode, uint64_t cluster_offset)
{
  struct Buf *buf;
  struct Pageframe *pf;
  struct SuperBlock *sb;
  uint32_t hash;
  vm_addr pa;

  while (1) {
    if ((buf = findblk(vnode, cluster_offset)) != NULL) {
      if (buf->flags & B_BUSY) {
        TaskSleep(&buf->rendez);
        continue;
      }

      buf->flags |= B_BUSY;

      if (buf->flags & (B_DELWRI | B_ASYNC)) {
        buf->flags &= ~(B_DELWRI | B_ASYNC);
        sb = buf->vnode->superblock;
  
        hash = buf->expiration_time % NR_DELWRI_BUCKETS;
        LIST_REM_ENTRY(&sb->delwri_timing_wheel[hash], buf, delwri_hash_link);
      }
      
      return buf;

    } else {
      if ((buf = LIST_HEAD(&buf_avail_list)) == NULL) {
        TaskSleep(&buf_list_rendez);
        continue;
      }

      LIST_REM_HEAD(&buf_avail_list, free_link);
      buf->flags |= B_BUSY;

      if (buf->flags & B_VALID) {
        LIST_REM_ENTRY(&buf_hash[buf->cluster_offset % BUF_HASH], buf, lookup_link);

        pmap_cache_extract((vm_addr)buf->data, &pa);
        pf = pmap_pa_to_pf(pa);
        pmap_cache_remove((vm_addr)buf->data);
        free_pageframe(pf);
      }

/*
      if (cluster_offset >= vnode->size) {
        cluster_size = 0;
      } else if (vnode->size - cluster_offset < CLUSTER_SZ) {
        cluster_size = ALIGN_UP(vnode->size - cluster_offset, PAGE_SIZE);
      } else {
        cluster_size = CLUSTER_SZ;
      }
*/

      for (int t = 0; t < (CLUSTER_SZ / PAGE_SIZE); t++) {
        pf = alloc_pageframe(PAGE_SIZE);
        pmap_cache_enter((vm_addr)buf->data + t * PAGE_SIZE, pf->physical_addr);
      }

      pmap_flush_tlbs();
      buf->flags &= ~B_VALID;
      buf->vnode = vnode;

      buf->cluster_offset = cluster_offset;
      LIST_ADD_HEAD(&buf_hash[buf->cluster_offset % BUF_HASH], buf,
                    lookup_link);

      return buf;
    }
  }
}


/* @brief   Release a cache block
 *
 * @param   buf, buf to be be released
 */
void brelse(struct Buf *buf)
{
  if (buf->flags & (B_ERROR | B_DISCARD)) {
    LIST_REM_ENTRY(&buf_hash[buf->cluster_offset % BUF_HASH], buf, lookup_link);
    buf->flags &= ~(B_VALID | B_ERROR);
    buf->cluster_offset = -1;
    buf->vnode = NULL;

    if (buf->data != NULL) {
      LIST_ADD_HEAD(&buf_avail_list, buf, free_link);
    }
  } else if (buf->data != NULL) {
    LIST_ADD_TAIL(&buf_avail_list, buf, free_link);
  }

  buf->flags &= ~B_BUSY;
  TaskWakeupAll(&buf_list_rendez);
  TaskWakeupAll(&buf->rendez);
}


/* @brief   Find a specific file's block in the cache
 *
 * @param   vnode, file to find block of
 * @param   cluster_offset, offset within the file (aligned to cluster size)
 * @return  buf on success, null if not present
 */
struct Buf *findblk(struct VNode *vnode, uint64_t cluster_offset)
{
  struct Buf *buf;

  buf = LIST_HEAD(&buf_hash[cluster_offset % BUF_HASH]);

  while (buf != NULL) {
    if (buf->vnode == vnode && buf->cluster_offset == cluster_offset)
      return buf;

    buf = LIST_NEXT(buf, lookup_link);
  }

  return NULL;
}


/* @brief   Read a block from a file
 *
 * @param   vnode, vnode of file to read
 * @param   cluster_offset, reads a block from disk into the cache
 * @return  Pointer to a Buf that represents a cached block or NULL on failure
 *
 * This searches for the block in the cache. if a block is not present
 * then a new block is allocated in the cache and if needed it's contents
 * read from disk.
 *
 * In the current implementation the cache's block size is 4 Kb.
 * A block read will be of this size. If the block is at the end of the
 * file the remaining bytes after the end will be zero.
 *
 */
struct Buf *bread(struct VNode *vnode, uint64_t cluster_base)
{
  struct Buf *buf;
  ssize_t xfered;

  buf = getblk(vnode, cluster_base);

  if (buf->flags & B_VALID) {
    return buf;
  }

  buf->flags = (buf->flags | B_READ) & ~(B_WRITE | B_ASYNC);

  xfered = vfs_read(vnode, buf->data, CLUSTER_SZ, &cluster_base);

  if (xfered > CLUSTER_SZ) {
    Error("bread > CLUSTER_SZ: %d", xfered);
  }

	if (xfered <= 0) {
		Error("bread error: %d", xfered);
		buf->flags |= B_ERROR;
	} else if (xfered != CLUSTER_SZ) {
		memset(buf->data + xfered, 0, CLUSTER_SZ - xfered);				
  }
  
  if (buf->flags & B_ERROR) {
    Error("block read failed");
    brelse(buf);
    return NULL;
  }

  buf->flags = (buf->flags | B_VALID) & ~B_READ;
  return buf;
}


/*
 *
 */
struct Buf *bread_zero(struct VNode *vnode, uint64_t cluster_base)
{
  struct Buf *buf;
  ssize_t xfered;

  buf = getblk(vnode, cluster_base);

  if (buf->flags & B_VALID) {
    Warn("bzero ok (cached) ???");
    return buf;
  }

  memset(buf->data, 0, CLUSTER_SZ);

  buf->flags = (buf->flags | B_VALID) & ~B_READ;

  return buf;
}


/* @brief   Writes a block to disk and releases it. Waits for IO to complete.
 * 
 * @param   buf, buffer to write
 * @return  0 on success, negative errno on failure
 *
 * FIXME: Work out file size here.  Send only upto file size.
 *
 * FIXME:File size is sent with the command, so it can write less than a cluster_sz to disk
 * Or we keep track of physical block size and how many physical blocks are valid
 * in cache block.  Harder for sparse files unless we have a bitmap.
 */
int bwrite(struct Buf *buf)
{
  ssize_t xfered;
  struct VNode *vnode;
  off64_t cluster_offset;
  
  buf->flags = (buf->flags | B_WRITE) & ~(B_READ | B_ASYNC);
  vnode = buf->vnode;
  cluster_offset = buf->cluster_offset;

  // FIXME: Only write a partial cluster if this is last cluster
  xfered = vfs_write(vnode, buf->data, CLUSTER_SZ, &cluster_offset);

  if (xfered != CLUSTER_SZ) {
    buf->flags |= B_ERROR;
  }
  if (buf->flags & B_ERROR) {
    brelse(buf);
    return -1;
  }

  buf->flags &= ~B_WRITE;
  brelse(buf);
  return 0;
}


/* @brief   Write buffer asynchronously.
 *
 * @param   buf, buffer to write
 * @return  0 on success, negative errno on failure
 *
 * Queue buffer for write immediately. This block is unlikely to be
 * written again anytime soon so write as soon as possible.
 *
 * This is called when a write will reach the end of the current block
 * and the next write will write another block.
 */
int bawrite(struct Buf *buf)
{
  struct SuperBlock *sb;
  struct VNode *vnode;
  uint32_t hash;

  vnode = buf->vnode;
  sb = vnode->superblock;
    
  buf->flags = (buf->flags | B_WRITE | B_ASYNC) & ~(B_READ | B_DELWRI);
  buf->expiration_time = sb->softclock;
  
  hash = buf->expiration_time % NR_DELWRI_BUCKETS;
  LIST_ADD_TAIL(&sb->delwri_timing_wheel[hash], buf, delwri_hash_link);
  brelse(buf);

  return 0;
}


/* @brief   Delayed write to buffer
 *
 * @param   buf, buffer to write
 * @return  0 on success, negative errno on failure
 *
 * Delay writing of a block for a few seconds. This block is likely
 * to be written again soon so schedule it to be written out after a
 * short delay.
 *
 * This is called when a write does not cross into the next block.
 * 
  // TODO: Insert onto delwri queue 5 seconds from now
  // If already on delayed write queue, reinsert it at time, so that
  // it will eventually get flushed, if time has passed, insert it at
  // the head so it will be written soon.
 */
int bdwrite(struct Buf *buf)
{
  struct SuperBlock *sb;
  struct VNode *vnode;
  uint32_t hash;

  vnode = buf->vnode;
  sb = vnode->superblock;

  buf->flags = (buf->flags | B_WRITE | B_DELWRI) & ~(B_READ | B_ASYNC);

  buf->expiration_time = sb->softclock + DELWRI_DELAY_TICKS;

  hash = buf->expiration_time % NR_DELWRI_BUCKETS;
  LIST_ADD_TAIL(&sb->delwri_timing_wheel[hash], buf, delwri_hash_link);
  brelse(buf);
  
  return 0;
}


/* @brief   Write out all cached blocks of a file
 *
 * @param   vnode, file to write dirty cached blocks to disk
 * @return  0 on success, negative errno on failure.
 */
int bsync(struct VNode *vnode)
{
  // Find all blocks in cache belonging to file, immediately write out.
  // What if blocks are busy, wait?
  
  // Do we notify the bdflush_task ?
  
  return -ENOSYS;
}


/* @brief   Write out all cached blocks of a mounted filesystem.
 * 
 * @param   sb, superblock of mounted filesystem to sync contents of
 * @return  0 on success, negative errno on failure
 */
int bsyncfs(struct SuperBlock *sb)
{
  // Do we notify the bdflush_task ?

  return -ENOSYS;
}


/* @brief   Resize contents of file in cache.
 * 
 * @param   vnode, file to resize
 * @return  0 on success, negative errno on failure
 *
 * The new size must already be set within the vnode structure
 * Delete bufs if needed. Erase partial buf of last block to avoid
 * leakage of past data.  
 */
int btruncate(struct VNode *vnode)
{
  return -ENOSYS;
}


/* @brief   Dynamically change the size of the filesystem cache
 */
size_t resize_cache(size_t free)
{
  // Find out how much we need
  // while (cache_shrink_busy)
  // cond_wait();

  // Flush LRU until

  return -ENOSYS;
}


/* @brief   Initialize a superblock's timing wheel and delayed-write message pool
 *
 */
int init_superblock_bdflush(struct SuperBlock *sb)
{
	Info("init_superblock_bdflush");
	
  sb->delwri_timing_wheel = kmalloc_page();
  
  if (sb->delwri_timing_wheel == NULL) {
    return -ENOMEM;
  }
    
  for (int t=0; t < NR_DELWRI_BUCKETS; t++) {
    LIST_INIT(&sb->delwri_timing_wheel[t]);
  }
  
  sb->softclock = get_hardclock();
}


/* @brief   Free delayed-write message pool and timing wheel
 *
 */
void deinit_superblock_bdflush(struct SuperBlock *sb)
{
  kfree_page(sb->delwri_timing_wheel);  
  sb->delwri_timing_wheel = NULL;
}


/* @brief   Periodically write dirty blocks on the delayed-write list to disk
 *
 * @param   arg, unused
 * @return  0 on success when task terminates, negative errno on failure
 */
int sys_bdflush(int fd)
{
  struct SuperBlock *sb;
  struct Buf *buf;
  uint64_t now;
	int count = 0;
	struct Process *current;
	
	Info("bdflush(%d)", fd);
	
	current = get_current_process();
	sb = get_superblock(current, fd);
	
	if (sb == NULL || sb->delwri_timing_wheel == NULL) {
	  Info ("bdflush bad fd");
		return -EBADF;
	}
	
	now = get_hardclock();
	
  while (sb->softclock < now) { 
    while((buf = find_delayed_write_buf(sb, sb->softclock)) != NULL) {
       vfs_write_async(sb, buf);
       count++;      
    }
   	
    sb->softclock++; // = BDFLUSH_SOFTCLOCK_TICKS;  FIXME: Need to quantise expire and softclock
  }

  Info("bdflush count:%d", count);
	return count;
}


/* @brief		Release any resources of a delayed write buffer during replymsg
 *
 *   // only do this if replyport is null

 */
int bdflush_brelse(struct Msg *msg)
{
  brelse((struct Buf *)msg);
}


/* @brief   find a block that has expired on the delayed write timing wheel
 *
 * @param   sb, superblock to get delayed write block for
 * @param   softclock_ticks,  softclock time to search for delwri blocks
 * @return  buf or NULL if no entries in the delayed-write queue
 *
 * This does not remove the Buf from the delayed write timing wheel and is
 * not marked as busy. The Buf is removed by calling unhash_delayed_write_buf().
 */
struct Buf *find_delayed_write_buf(struct SuperBlock *sb, uint64_t softclock)
{
  struct Buf *buf;
  uint32_t hash;
  
  hash = softclock % NR_DELWRI_BUCKETS;
  
  buf = LIST_HEAD(&sb->delwri_timing_wheel[hash]);
  
  while (buf != NULL) {  
    if (buf->expiration_time <= softclock) {
		  LIST_REM_ENTRY(&sb->delwri_timing_wheel[hash], buf, delwri_hash_link);
		  buf->flags |= B_BUSY;
      return buf;
    }
    
    LIST_NEXT(buf, delwri_hash_link);
  }
  
  return NULL;
}


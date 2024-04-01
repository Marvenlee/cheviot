/*
 * Copyright 2023  Marven Gilhespie
 *
 * Licensed under the Apache License, segment_id 2.0 (the "License");
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

/* Copied from FatFS code. Block Cache is legacy of old Kielder OS code,
 * Move into a blockcache library and cleanup.
 */

#define LOG_LEVEL_WARN

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/blockdev.h>
#include <sys/fsreq.h>
#include <sys/mount.h>
#include <sys/panic.h>
#include <sys/stat.h>
#include <sys/syscalls.h>
#include <sys/debug.h>
#include <unistd.h>


/* @brief   Initialize the block cache
 *
 * @param   dev_fd, handle to block device to read and write from
 * @param   buf_cnt, number of blocks to hold in cache
 * @param   block_size, size of blocks used by file system
 * @return  pointer to block_cache structure or NULL on failure
 */
struct block_cache *init_block_cache(int dev_fd, int buf_cnt, size_t block_size)
{
  struct block_cache *cache;
  
 	log_debug("init_block_cache(buf_cnt:%d, block_size:%u", buf_cnt, block_size);
  
	if (buf_cnt == 0 || block_size < 512) {
		panic("bad params to init_block_cache");
	}
	
	if ((cache = malloc (sizeof (struct block_cache))) != NULL) {
		if ((cache->buf_table = (struct buf *)virtualalloc(NULL, buf_cnt * sizeof (struct buf), 
		                                                   PROT_READWRITE)) != NULL) {
			if ((cache->mem_pool = (char *)virtualalloc(NULL, buf_cnt * block_size, 
			                                          PROT_READWRITE)) != NULL) {			
				cache->dev_fd = dev_fd;
				
				cache->buf_cnt = buf_cnt;
				cache->block_size = block_size;
								
				LIST_INIT (&cache->lru_list);
				LIST_INIT (&cache->free_list);
												
				for (int t=0; t < BUF_HASH_CNT; t++) {
					LIST_INIT (&cache->hash_list[t]);
				}
					
				for (int t=0; t < buf_cnt; t++) {
					cache->buf_table[t].block = 0;
					cache->buf_table[t].data = (uint8_t *)cache->mem_pool + (t * block_size);
					cache->buf_table[t].valid = false;
					cache->buf_table[t].dirty = false;
					cache->buf_table[t].in_use = true;
													
					LIST_ADD_TAIL(&cache->free_list, &cache->buf_table[t], free_link);
				}
				
				return cache;
			}
		}
	}
	
	return NULL;
}


/* @brief   Free the resources associated with this block cache
 *
 */
void free_cache(struct block_cache *cache)
{
	virtualfree(cache->mem_pool, cache->buf_cnt * cache->block_size);
	virtualfree(cache->buf_table, cache->buf_cnt * sizeof (struct buf));
	free(cache);
}


/* @brief   Get a block from the cache, reading it from media if necessary
 *
 * @param   cache, the cache the block belongs to 
 * @param   opt, controls how the block is read
 *          BLK_READ    = Normal operation, block is read from disk if it is
 *                        not already in the cache.
 *          BLK_NO_READ = allocate the buffer but do not read from disk.
 *                        The caller will either overwrite the buffer with a
 *                        full block of data for writing or will clear the
 *                        entire buffer of the block.
 *          BLK_CLEAR   = allocate the buffer and clear it. Do not read from disk.
 * @return  pointer to a buf structure or NULL on failure.
 */
struct buf *get_block(struct block_cache *cache, off64_t block, int opt)
{
	struct buf *buf;
	uint32_t hash;
	int rc;
		
	hash = block % BUF_HASH_CNT;
	buf = LIST_HEAD (&cache->hash_list[hash]);

	while (buf != NULL) {		
		if (buf->block == block) {			
			break;
		}
		
		buf = LIST_NEXT (buf, hash_link);
	}

						
	if (buf == NULL) {
		buf = LIST_HEAD (&cache->free_list);
						
		if (buf != NULL) {
	  	LIST_REM_HEAD (&cache->free_list, free_link);
			assert(buf->valid == false);
  			
		} else {
			buf = LIST_HEAD (&cache->lru_list);

			if (buf == NULL) {
				panic("libblockdev: no bufs available");
			}			

			// if it's on the LRU list it is valid, it is also hashed.
			assert(buf->valid == true);
			
			// Put block should immediately write buffer, so it should never be dirty.
			assert(buf->dirty == false);

			hash = buf->block % BUF_HASH_CNT;
			LIST_REM_HEAD (&cache->lru_list, lru_link);
			LIST_REM_ENTRY (&cache->hash_list[hash], buf, hash_link);
		}
		
		buf->block = block;
		buf->dirty = false;
		buf->in_use = true;

		uint32_t *beef = buf->data;
		for (int t=0; t <cache->block_size/4; t++) {
			beef[t]=0xdeadbeef;
		}
	  
		hash = buf->block % BUF_HASH_CNT;
		LIST_ADD_HEAD (&cache->hash_list[hash], buf, hash_link);

    if (opt == BLK_READ) {
      lseek64(cache->dev_fd, (uint64_t)block * cache->block_size, SEEK_SET);
	    rc = read(cache->dev_fd, buf->data, cache->block_size);

			if (rc != cache->block_size) {
				log_warn("libblockdev: get_block read rc:%d != sz %d", rc, cache->block_size);
			}

			buf->valid = true;
	  } else if (opt == BLK_NO_READ) {
      /* Returning a buf without reading its contents and without clearing
       * it first. */
    } else if (opt == BLK_CLEAR) {
      memset(buf->data, 0, cache->block_size);
    } else {
      panic("libblockdev: get_block unknown option");
    }
    
		return buf;
		
	} else {
	  buf->in_use = true;
		return buf;
	}
}


/* @brief   Release a cached block, depending oo flags write block if dirty
 *
 * @param   cache, the cache the block belongs to 
 * @param   buf, cached block to release 
 */
void put_block(struct block_cache *cache, struct buf *buf)
{
	if (buf->in_use == false) {
		log_error("libblockdev: put_block of not in use blk:%u, buf:%08x", 
		          (uint32_t)buf->block, (uint32_t)buf);
	}

  if (block_isclean(buf) == false) {  
    lseek64(cache->dev_fd, (uint64_t)buf->block * cache->block_size, SEEK_SET);
		write(cache->dev_fd, buf->data, cache->block_size);
  }
  
  block_markclean(buf);

	LIST_ADD_TAIL (&cache->lru_list, buf, lru_link);
	buf->in_use = false;  
}


/* @brief   Remove block from cache but do not flush it to disk
 *
 * @param   block, block to invalidate
 */
void invalidate_block(struct block_cache *cache, off64_t block)
{
  struct buf *buf;
  uint32_t hash;
  
	hash = block % BUF_HASH_CNT;
	buf = LIST_HEAD (&cache->hash_list[hash]);
		
	while (buf != NULL) {		
		if (buf->block == block) {
      // Remove from cache
      assert(buf->valid == true);
      
	    LIST_REM_ENTRY (&cache->lru_list, buf, lru_link);
	    LIST_REM_ENTRY (&cache->hash_list[hash], buf, hash_link);
	    LIST_ADD_TAIL (&cache->free_list, buf, free_link);
      buf->dirty = false;
      buf->in_use = false;
      buf->valid = false;
	    return;
		}
		
		buf = LIST_NEXT (buf, hash_link);
	}
}


/* @brief   Mark as block in the cache as dirty, write out in put_block
 *
 * @param   buf, buffer to mark as dirty
 */
void block_markdirty(struct buf *bp)
{
  bp->dirty = true;  
}


/* @brief   Mark as block in the cache as clean (doesn't need to be written to disk)
 *
 * @param   buf, buffer to mark as clean
 */
void block_markclean(struct buf *bp)
{
  bp->dirty = false;
}


/* @brief   Determine whether block in cache is clean (doesn't need to written)
 *
 * @param   buf, buffer to check
 * @return  true if buffer is clean, false if buffer is dirty (needs to be written to disk)
 */
int block_isclean(struct buf *bp)
{
  return (bp->dirty == false);
}




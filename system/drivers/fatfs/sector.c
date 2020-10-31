/*
 * Copyright 2014  Marven Gilhespie
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

/* Need to simplify this file.  Block Cache is legacy of old Kielder OS code,
 * is it needed if we have a file cache in kernel.
 */

#include "fat.h"
#include "globals.h"
#include "sys/debug.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fsreq.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/syscalls.h>
#include <unistd.h>


static int BufReadSector (struct Cache *cache, struct Blk *blk, uint32_t sector);
static int BufWriteSector (struct Cache *cache, struct Blk *blk, int mode);


int readSector(void *mem, int sector, int sector_offset, size_t nbytes) {
  return BufReadBlocks (block_cache, mem, sector+fsb.partition_start, sector_offset, nbytes);
}

int writeSector(void *mem, int sector, int sector_offset, size_t nbytes) {
  return BufWriteBlocks (block_cache, mem, sector+fsb.partition_start, sector_offset, nbytes, 0);
}

int blockRead(void *mem, size_t sz, int block_no) {
  BufReadBlocks (block_cache, mem, block_no, 0, sz);    
  return 512;
}

int blockWrite(void *mem, size_t sz, int block_no) {
  BufWriteBlocks (block_cache, mem, block_no, 0, sz, 0);  
  return 512;
}

struct Cache *CreateCache (int block_fd, uint32_t buffer_cnt, uint32_t block_size,
						uint32_t lba_start, uint32_t lba_end, int writethru_critical,
						uint32_t writeback_delay, uint32_t max_transfer)
{
	struct Cache *cache;
	int t;
	
	
	if (buffer_cnt == 0 || block_size < 512)
		return NULL;
		
	if ((cache = malloc (sizeof (struct Cache))) != NULL)
	{
		if ((cache->blk_table = malloc (sizeof (struct Blk) * buffer_cnt)) != NULL)
		{
			if ((cache->blk_mem = malloc (buffer_cnt * block_size)) != NULL)
			{
				cache->block_fd = block_fd;
				
				cache->buffer_cnt = buffer_cnt;
				cache->block_size = block_size;
				cache->lba_start = lba_start;
				cache->lba_end = lba_end;
				cache->writethru_critical = writethru_critical;
				cache->writeback_delay = writeback_delay;
				cache->max_transfer = max_transfer;
				
				LIST_INIT (&cache->lru_list);
				LIST_INIT (&cache->dirty_list);
				LIST_INIT (&cache->free_list);
				
								
				for (t=0; t < BUF_HASH_CNT; t++)
				{
					LIST_INIT (&cache->hash_list[t]);
				}
					
				for (t=0; t < buffer_cnt; t++)
				{
					cache->blk_table[t].cache = cache;
					cache->blk_table[t].sector = -1;
					cache->blk_table[t].dirty = false;
					cache->blk_table[t].mem = (uint8_t *)cache->blk_mem + (t * block_size);
					cache->blk_table[t].in_use = false;
													
					LIST_ADD_TAIL(&cache->free_list, &cache->blk_table[t], free_entry);
				}
				
				return cache;
			}
		}
	}
	
	return NULL;
}

/*
 *
 */
void FreeCache (struct Cache *cache)
{
	free (cache->blk_mem);
	free (cache->blk_table);
	free (cache);
}

/*
 * SyncBuf();
 *
 * Flush all dirty blocks out to disk.  Intended to be called by the handler task
 * in response to a periodic timer of several seconds.
 */
void SyncCache (struct Cache *cache)
{
	struct Blk *blk;
	
	while ((blk = LIST_HEAD (&cache->dirty_list)) != NULL)
	{
		blk->dirty = false;
		LIST_REM_HEAD (&cache->dirty_list, dirty_entry);
							
		BufWriteSector (cache, blk, BUF_IMMED);
	}
}

/*
 *
 */
void InvalidateCache (struct Cache *cache)
{
	struct Blk *blk;
	int hash_idx;
	int t;
	
	
	for (t=0; t < cache->buffer_cnt; t++)
	{
		blk = &cache->blk_table[t];
		
		if (blk->in_use == true)
		{
			hash_idx = blk->sector % BUF_HASH_CNT;
		
			LIST_REM_ENTRY (&cache->lru_list, blk, lru_entry);
			LIST_REM_ENTRY (&cache->hash_list[hash_idx], blk, hash_entry);
			LIST_ADD_HEAD (&cache->free_list, blk, free_entry);
			
			blk->in_use = false;
			blk->dirty = false;
			blk->sector = 0;
		}
	}
}




/*
 * BufReadBlocks();
 */

int BufReadBlocks (struct Cache *cache, void *addr,	uint32_t block, uint32_t offset, uint32_t nbytes)
{
	struct Blk *blk;
	uint32_t nbytes_to_read;
	uint32_t remaining_in_block;
	void *cache_buf;
	int rc = 0;
	
	
	/* Need to limit read upto end of media */

	while (nbytes > 0)
	{
		remaining_in_block = cache->block_size - offset;
		
		nbytes_to_read = (nbytes < remaining_in_block) ? nbytes : remaining_in_block;
		
		if ((blk = BufGetBlock (cache, block)) == NULL)
		{
			rc = -1;
			break;
		}
		
		cache_buf = blk->mem;
//	  KLog ("fat: buf_read_blocks dst = %08x, src = %08x, sz = %d offs = %d", (uint32)addr, (uint32_t)((uint8_t *)cache_buf + offset),
//	                nbytes_to_read, offset);
		memcpy (addr, (uint8_t *)cache_buf + offset, nbytes_to_read);
		
		nbytes -= nbytes_to_read;
		addr += nbytes_to_read;
					
		block++;
		offset = 0;
	}
		
	return rc;
}

/*
 * WriteBlocks();
 */
int BufWriteBlocks (struct Cache *cache, void *addr, uint32_t block, uint32_t offset, uint32_t nbytes, int mode)

{
	struct Blk *blk;
	uint32_t nbytes_to_write;
	uint32_t remaining_in_block;
	void *cache_buf;
	int rc = 0;
	
	
	while (nbytes > 0)
	{
		remaining_in_block = cache->block_size - offset;
		
		nbytes_to_write = (nbytes < remaining_in_block) ? nbytes : remaining_in_block;
		
		
		if ((blk = BufGetBlock (cache, block)) == NULL)
		{
			rc = -1;
			break;
		}
		
		cache_buf = blk->mem;
		
		
		if (addr != NULL)
		{
			memcpy ((uint8 *)cache_buf + offset, addr, nbytes_to_write);
		}
		else
		{
			memset ((uint8 *)cache_buf + offset, 0, nbytes_to_write); 
		}
		
		if (BufPutBlock (cache, blk, mode) == NULL)
		{
			rc = -1;
			break;
		}
		
		nbytes -= nbytes_to_write;
		addr += nbytes_to_write;
		block++;
		offset = 0;
	}

	return rc;
}

/*
 * GetBlock();
 */
struct Blk *BufGetBlock (struct Cache *cache, uint32_t block)
{
	struct Blk *blk;
	uint32_t hash_idx;
	int rc;
	
	/* Need to limit read upto end of media, final block may be partial,
	 * simply use disksize%blocksize for final block? */
		
	hash_idx = block % BUF_HASH_CNT;

	blk = LIST_HEAD (&cache->hash_list[hash_idx]);

		
	while (blk != NULL)
	{		
		if (blk->sector == block)
			break;
		
		blk = LIST_NEXT (blk, hash_entry);
	}
				
	if (blk == NULL)
	{
		blk = LIST_HEAD (&cache->free_list);
						
		if (blk == NULL)
		{
			blk = LIST_TAIL (&cache->lru_list);

			if (blk == NULL)
			{
				KLog ("blk not found");
				exit(-1);
			}
			
							
			if (blk->dirty == true)
			{
				blk->dirty = false;
				LIST_REM_ENTRY (&cache->dirty_list, blk, dirty_entry);
							
				if (BufWriteSector (cache, blk, BUF_IMMED) != 0)
				{
					KLog ("BufWriteBlock failure");
					exit(-1);
				}
			}
			
						
			hash_idx = blk->sector % BUF_HASH_CNT;
			
			LIST_REM_ENTRY (&cache->lru_list, blk, lru_entry);
			LIST_REM_ENTRY (&cache->hash_list[hash_idx], blk, hash_entry);
			
			blk->in_use = false;
			LIST_ADD_HEAD (&cache->free_list, blk, free_entry);
		}

		
		LIST_REM_HEAD (&cache->free_list, free_entry);
		blk->in_use = true;
		
		LIST_ADD_HEAD (&cache->lru_list, blk, lru_entry);
		blk->sector = block;
		blk->dirty = false;
		blk->cache = cache;
		
				
		hash_idx = blk->sector % BUF_HASH_CNT;
		LIST_ADD_HEAD (&cache->hash_list[hash_idx], blk, hash_entry);
		
				
		rc = BufReadSector (cache, blk, block);
				
		if (rc == 0)
		{
			return blk;
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		LIST_REM_ENTRY(&cache->lru_list, blk, lru_entry);
		LIST_ADD_HEAD(&cache->lru_list, blk, lru_entry);
						
		return blk;
	}
	
}

/*
 * PutBlock();
 *
 * "mode" determines where on the LRU list the block is written and if it
 * should be written immediately.
 *
 * Values inside "buf" determine whether a block is written to disk or left
 * for the caller to write blocks to disk by periodically calling SyncBuf().
 *
 * If writeback_delay == 0    write EVERYTHING IMMEDIATELY
 * If writethru_critical == 1  write BUF_IMMED blocks immediately.
 */
struct Blk *BufPutBlock (struct Cache *cache, struct Blk *blk, int mode)
{
	int rc;

	if (mode & BUF_ONESHOT)
	{
		LIST_REM_ENTRY (&cache->lru_list, blk, lru_entry)
		LIST_ADD_TAIL (&cache->lru_list, blk, lru_entry);
	}
	
	

	if (mode & BUF_IMMED)
	{
		if (blk->dirty == true)
		{	
			blk->dirty = false;
			LIST_REM_ENTRY (&cache->dirty_list, blk, dirty_entry);
		}
	
		if (cache->writethru_critical != 0)
		{
			rc = BufWriteSector (cache, blk, BUF_IMMED);
		
			if (rc == 0)
			{
				return blk;
			}
			else
			{
//				SetError (EIO);
				return NULL;
			}
		}
		
		return blk;
	}
	else
	{
		/* Mark as dirty and place on tail of dirty list */
				
		if (blk->dirty != true)
		{
			blk->dirty = true;
			LIST_ADD_TAIL (&cache->dirty_list, blk, dirty_entry);
		}
		
		if (cache->writeback_delay == 0)
		{
			rc = BufWriteSector (cache, blk, BUF_IMMED);
		
			if (rc == 0)
			{
				return blk;
			}
			else
			{
//				SetError (EIO);
				return NULL;
			}
		}
				
		return blk;
	}
}

/*
 *
 */
static int BufReadSector (struct Cache *cache, struct Blk *blk, uint32_t sector)
{
  size_t nbytes_read = 0;
  off64_t offset;

  offset = (off64_t)sector * 512;
  Seek64(block_fd, &offset, SEEK_SET);

  nbytes_read = read(block_fd, blk->mem, 512);

  if (nbytes_read < 0) {
    KLog("BlockRead %d, exiting", nbytes_read);
    exit(-1);
    return -EIO;
  }

  return 0;
}

/*
 *
 */
static int BufWriteSector (struct Cache *cache, struct Blk *blk, int mode)
{
  size_t nbytes_written = 0;
  off64_t offset;

  offset = (off64_t)blk->sector * 512;
  Seek64(block_fd, &offset, SEEK_SET);

  nbytes_written = write(block_fd, blk->mem, 512);

  if (nbytes_written < 0) {
    KLog("BlockWrite %d, exiting", nbytes_written);
    exit(-1);
    return -EIO;
  }

  return 0;
}






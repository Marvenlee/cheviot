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

#ifndef SYS_BLOCKDEV_H
#define SYS_BLOCKDEV_H

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/lists.h>
#include <sys/stat.h>


/*
 * get_block flag
 */
#define BLK_READ              0             /* get_block will read from disk if needed */
#define BLK_NO_READ           1             /* get_block will not read contents from disk */
#define BLK_CLEAR             2             /* get_block will return a clear block */

// Number of buckets in buf hash table
#define BUF_HASH_CNT  128

/*
 * Types
 */
typedef uint32_t block_t;
typedef uint64_t block64_t;

LIST_TYPE(buf, buf_list_t, buf_link_t);


/*
 * @brief   Manages the block cache
 */
struct block_cache
{
  void *mem_pool;
  size_t mem_pool_sz;

  int dev_fd;
    
  size_t block_size;
  
  off64_t partition_start_sector;
  int sector_size;

  off64_t lba_start;
  off64_t lba_end;
  
  //  size_t sector_size;
  //  size_t sectors_per_block;
  
  struct buf *buf_table;
  int buf_cnt;

  buf_list_t free_list;
  buf_list_t lru_list;
  buf_list_t hash_list[BUF_HASH_CNT];
};


/*
 * @brief   Manages a cached block in memory
 */
struct buf
{
  off_t block;
  void *data;
  uint32_t flags;
  
  bool in_use;
  bool valid;
  bool dirty;
  
  buf_link_t free_link;
  buf_link_t lru_link;
  buf_link_t hash_link;
};


/*
 * Prototypes
 */ 

// block_cache.c
struct block_cache *init_block_cache(int dev_fd, int buf_cnt, size_t block_size);
struct buf *get_block(struct block_cache *cache, off64_t block, int opt);
void put_block(struct block_cache *cache, struct buf *buf);
void invalidate_block(struct block_cache *cache, off64_t block);
void block_markdirty(struct buf *bp);
void block_markclean(struct buf *bp);
int block_isclean(struct buf *bp);



#endif


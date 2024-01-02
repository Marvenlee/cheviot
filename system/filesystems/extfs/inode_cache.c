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

/* @brief   Inode cache functions
 * This is partially based on the Minix Ext2 FS inode sources
 */
 
#define LOG_LEVEL_INFO

#include "ext2.h"
#include "globals.h"


/* @brief   Initialize the inode cache
 *
 */
int init_inode_cache(void)
{
  LIST_INIT(&unused_inode_list);
  
  for (int t=0; t< NR_INODES; t++) {
  	inode_cache[t].i_ino = NO_ENTRY;
  	LIST_ADD_TAIL(&unused_inode_list, &inode_cache[t], i_unused_link);
  }
  
  for (int t=0; t< INODE_HASH_SIZE; t++) {
  	LIST_INIT(&hash_inodes[t]);
  }
  
  return 0;
}


/* @brief   Add an inode to the cache's hash table
 *
 */
void addhash_inode(struct inode *inode)
{
  int h;
  
  h = inode->i_ino % INODE_HASH_SIZE;
  LIST_ADD_HEAD(&hash_inodes[h], inode, i_hash_link);
}


/* @brief   Remove an inode from the cache's hash table
 *
 */
void unhash_inode(struct inode *inode)
{
  int h;
  
  h = inode->i_ino % INODE_HASH_SIZE;
  LIST_REM_ENTRY(&hash_inodes[h], inode, i_hash_link);
}


/* @brief   Get an inode from the cache and if needed fetch from disk
 *
 * @param   ino_nr, inode number of inode to get
 * @return  pointer to inode structure or NULL on failure 
 *
 * Find the inode in the hash table. If it is not there, get a free inode
 * load it from the disk if it's necessary and put on the hash list
 */
struct inode *get_inode(ino_t ino_nr)
{
  struct inode *inode;
  int h;

  h = (int) ino_nr % INODE_HASH_SIZE;
  inode = LIST_HEAD(&hash_inodes[h]);
      
  while (inode != NULL) {
	  if (inode->i_ino == ino_nr) {
		  if (inode->i_count == 0) {
			  LIST_REM_ENTRY(&unused_inode_list, inode, i_unused_link);
		  }
		  
		  inode->i_count++;
		  return inode;
	  }
	  
	  inode = LIST_NEXT(inode, i_hash_link);
  }

  if (LIST_EMPTY(&unused_inode_list)) {
  	return NULL;
  }

  inode = LIST_HEAD(&unused_inode_list);

  if (inode->i_ino != NO_ENTRY) {
  	unhash_inode(inode);
  }
  
  LIST_REM_HEAD(&unused_inode_list, i_unused_link);

  inode->i_ino = ino_nr;
  inode->i_count = 1;

  read_inode(inode);

  inode->i_update = 0;
  
  addhash_inode(inode);

  return inode;
}


/* @brief   Find an existing inode in the inode cache
 * 
 * @param   ino_nr, inode number of inode to find in the cache
 * @return  pointer to inode in cache, or NULL if not present
 *
 * This does not read the inode from disk.
 */
struct inode *find_inode(ino_t ino_nr)
{
  struct inode *inode;
  int h;

  h = (int) ino_nr % INODE_HASH_SIZE;
  inode = LIST_HEAD(&hash_inodes[h]);
  
  while (inode != NULL) {
	  if (inode->i_ino == ino_nr) {
		  return inode;
	  }
	  
	  inode = LIST_NEXT(inode, i_hash_link);
  }

  return NULL;
}


/* @brief   Release an inode back to the cache and if required flush to disk
 *
 * @param   inode, pointer to the inode to be released
 */
void put_inode(struct inode *inode)
{
  if (inode == NULL) {
  	return;
  }
  
  if (inode->i_count < 1) {
  	panic("extfs: put_inode: i_count already below 1");
  }
  
  if (--inode->i_count == 0) {
	  if (inode->i_links_count == 0) {
		  truncate_inode(inode, (off_t) 0);
		  inode_markdirty(inode);
		  free_inode(inode);
	  }
	  
	  if (inode->i_dirty == true) {
	    write_inode(inode);
    }
    
	  if (inode->i_links_count == 0) {
		  unhash_inode(inode);
		  inode->i_ino = NO_ENTRY;
		  LIST_ADD_HEAD(&unused_inode_list, inode, i_unused_link);
	  } else {
		  LIST_ADD_TAIL(&unused_inode_list, inode, i_unused_link);
	  }
  } else {
	  if (inode->i_dirty == true) {
	    write_inode(inode);
    }  
  }
}


/* @brief   Update the times in an inode
 *
 * @param   inode, pointer to inode to which timestamps will be updated
 *
 * Various system calls are required by the standard to update atime, ctime,
 * or mtime.  Since updating a time requires sending a message to the clock
 * task--an expensive business--the times are marked for update by setting
 * bits in i_update. When a stat, fstat, or sync is done, or an inode is
 * released, update_times() may be called to actually fill in the times.
 */
void update_times(struct inode *inode)
{
  time_t cur_time;

  if (sb_rd_only) {
  	return;
  }
  
  cur_time = time(NULL);

  if (inode->i_update & ATIME) {
  	inode->i_atime = cur_time;
  }
  
  if (inode->i_update & CTIME) {
  	inode->i_ctime = cur_time;
  }
  
  if (inode->i_update & MTIME) {
  	inode->i_mtime = cur_time;
  }
  
  inode->i_update = 0;
}


/* @brief   Read or Write the an inode to or from disk
 *
 * @param   inode, pointer to inode
 * @param   rw_flag, direction to read or write
 */
void read_inode(struct inode *inode)
{
  struct buf *bp;
  struct group_desc *gd;
  struct inode *dir_inode;
  uint32_t block_group_number;
  block_t b, offset;

  block_group_number = (inode->i_ino - 1) / superblock.s_inodes_per_group;
  gd = get_group_desc(block_group_number);

  if (gd == NULL) {
  	panic("extfs: can't get group_desc to read/ inode");
  }
  
  offset = ((inode->i_ino - 1) % superblock.s_inodes_per_group) * sb_inode_size;
  b = (block_t) gd->g_inode_table + (offset >> sb_blocksize_bits);
  
  bp = get_block(cache, b, BLK_READ);

  offset &= (sb_block_size - 1);
  dir_inode = (struct inode*) (bp->data + offset);

  inode_copy(inode, dir_inode);
  
  log_info("inode nr = %d", inode->i_ino);
  log_info("inode->i_mode   = %08x", inode->i_mode);
  log_info("inode->i_size   = %08x", inode->i_size);
  log_info("inode->i_uid    = %08x", inode->i_uid);
  log_info("inode->i_atime  = %08x", inode->i_atime);
  log_info("inode->i_ctime  = %08x", inode->i_ctime);
  log_info("inode->i_mtime  = %08x", inode->i_mtime);
  log_info("inode->i_gid    = %08x", inode->i_gid);
  log_info("inode->i_blocks = %08x", inode->i_blocks);
  log_info("inode->i_flags  = %08x", inode->i_flags);

  put_block(cache, bp);
}


/* @brief   Write an inode to disk
 *
 * @param   inode, pointer to inode
 * @param   rw_flag, direction to read or write
 */
void write_inode(struct inode *inode)
{
  struct buf *bp;
  struct group_desc *gd;
  struct inode *dir_inode;
  uint32_t block_group_number;
  block_t b, offset;

  block_group_number = (inode->i_ino - 1) / superblock.s_inodes_per_group;
  gd = get_group_desc(block_group_number);

  if (gd == NULL) {
  	panic("extfs: can't get group_desc to write inode");
  }
  
  offset = ((inode->i_ino - 1) % superblock.s_inodes_per_group) * sb_inode_size;
  b = (block_t) gd->g_inode_table + (offset >> sb_blocksize_bits);
  
  bp = get_block(cache, b, BLK_READ);

  offset &= (sb_block_size - 1);
  dir_inode = (struct inode*) (bp->data + offset);

  if (inode->i_update) {
	  update_times(inode);
	}

  inode_copy(dir_inode, inode);
  	  
  if (sb_rd_only == false) {
	  block_markdirty(bp);
	}
  
  put_block(cache, bp);
  inode_markclean(inode);
}



/* @brief   Copy on-disk inode structure in RAM and optionally swap bytes
 *
 * @param   inode, pointer to the in-core inode struct
 * @param   dir_inode, pointer to the on-disk struct
 */
void inode_copy(struct inode *dst, struct inode *src)
{
  dst->i_mode         = bswap2(be_cpu, src->i_mode);
  dst->i_uid          = bswap2(be_cpu, src->i_uid);
  dst->i_size         = bswap4(be_cpu, src->i_size);
  dst->i_atime        = bswap4(be_cpu, src->i_atime);
  dst->i_ctime        = bswap4(be_cpu, src->i_ctime);
  dst->i_mtime        = bswap4(be_cpu, src->i_mtime);
  dst->i_dtime        = bswap4(be_cpu, src->i_dtime);
  dst->i_gid          = bswap2(be_cpu, src->i_gid);
  dst->i_links_count  = bswap2(be_cpu, src->i_links_count);
  dst->i_blocks	      = bswap4(be_cpu, src->i_blocks);
  dst->i_flags	      = bswap4(be_cpu, src->i_flags);

  memcpy(&dst->osd1, &src->osd1, sizeof(dst->osd1));

  for (int i = 0; i < EXT2_N_BLOCKS; i++) {
	  dst->i_block[i]   = bswap4(be_cpu, src->i_block[i]);
  }
  
  dst->i_generation   = bswap4(be_cpu, src->i_generation);
  dst->i_file_acl	    = bswap4(be_cpu, src->i_file_acl);
  dst->i_dir_acl      = bswap4(be_cpu, src->i_dir_acl);
  dst->i_faddr	      = bswap4(be_cpu, src->i_faddr);

  memcpy(&dst->osd2, &src->osd2, sizeof(dst->osd2));
}


/*
 *
 */
void inode_markdirty(struct inode *inode)
{
  inode->i_dirty = true;
}


/*
 *
 */
void inode_markclean(struct inode *inode)
{
  inode->i_dirty = false;
}



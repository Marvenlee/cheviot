/* This file manages block allocation and deallocation.
 *
 * Created:
 *   June 2010 (Evgeniy Ivanov)
 *
 * Updated (CheviotOS Filesystem Handler based)
 *   December 2023 (Marven Gilhespie) 
 */

#define LOG_LEVEL_INFO

#include "ext2.h"
#include "globals.h"


/* @brief   Acquire a new block for an inode
 *
 * @param   inode, inode of file to allocate block for
 * @param   position, byte offset within the file to allocate block for
 * @return  cached buf of clear new block or NULL on failure with errno set.
 *
 * TODO: Save last allocated block position in inode and use that as a goal.
 */
struct buf *new_block(struct inode *inode, off_t position)
{
  struct buf *bp;
  block_t block;
  int sc;

  if ( (block = read_map_entry(inode, position)) == NO_BLOCK) {
	  block_t goal = NO_BLOCK;

	  if ((block = alloc_block(inode, goal)) == NO_BLOCK) {
		  log_warn("extfs: no space\n");
		  return NULL;
	  }
	  if ((sc = enter_map_entry(inode, position, block)) != 0) {
		  free_block(block);
		  log_warn("extfs: write_map failed, sc:%d", sc);
		  return NULL;
	  }
  }

  if ((bp = get_block(cache, block, BLK_CLEAR)) == NULL) {
  	panic("extfs: error getting block:%u, sc:%d", block, sc);
  }
  
  return bp;
}


/* @brief   Get the block number for a give inode and position in file
 *
 * @param   inode, ptr to inode to map from 
 * @param   position, position in file whose blk wanted
 *
 * Given an inode and a position within the corresponding file, locate the
 * block number in which that position is to be found and return it.
 */
block_t read_map_entry(struct inode *inode, uint64_t position)
{
  struct buf *bp;
  block_t block;
  uint32_t block_pos;
  uint32_t offs[4];
  int depth;
  
  block_pos = position / sb_block_size;
  depth = calc_block_indirection_offsets(block_pos, offs);

  if (depth < 0) {
    return NO_BLOCK;
  } else if (depth == 0) {
    return inode->i_block[offs[0]];   
  } 
  
  block = get_toplevel_indirect_block_entry(inode, depth);
        
  for (int t=1; t <= depth && block != NO_BLOCK; t++) {
    bp = get_block(cache, block, BLK_READ);    
    block = read_indirect_block_entry(bp, offs[t]);    
    put_block(cache, bp);
  }
  
  return block;
}


/* Write a new block into the inode's block map
 *
 * @param   inode, inode whose block map to add a block to
 * @param   position, offset in the file to allocate a new block for
 * @param   new_block, block number to be inserted
 * @return  0 on success, negative errno on failure
 */
int enter_map_entry(struct inode *inode, off_t position, block_t new_block)
{
  uint32_t block_pos;
  uint32_t offs[4];
  int depth;
  block_t block;
  struct buf *bp;
  struct buf *new_bp;
  
  block_pos = position / sb_block_size;
  depth = calc_block_indirection_offsets (block_pos, offs);

  if (depth < 0) {
    return -EINVAL;
  }
  
  inode_markdirty(inode);

  if (depth == 0) {
    inode->i_block[offs[0]] = new_block;
    inode->i_blocks += sb_sectors_in_block;
    return 0;    
  }

  block = get_toplevel_indirect_block_entry(inode, depth);  
  
  if (block == NO_BLOCK) {
    block = alloc_block(inode, NO_BLOCK);
    
    if (block == NO_BLOCK) {
      return -ENOSPC;
    }

    bp = get_block(cache, block, BLK_CLEAR);
    block_markdirty(bp);
    put_block(cache, bp);

    set_toplevel_indirect_block_entry(inode, depth, block);
    inode->i_blocks += sb_sectors_in_block;
  }
        
  for (int t=1; t < depth; t++) {
    bp = get_block(cache, block, BLK_READ);    
    block = read_indirect_block_entry(bp, offs[t]);    

    if (block == NO_BLOCK) {
      block = alloc_block(inode, NO_BLOCK);
      
      if (block == NO_BLOCK) {
        return -ENOSPC;
      }

      new_bp = get_block(cache, block, BLK_CLEAR);
      block_markdirty(new_bp);
      put_block(cache, new_bp);
      
      write_indirect_block_entry(bp, offs[t], block);
      block_markdirty(bp);

      inode->i_blocks += sb_sectors_in_block;
    }  

    put_block(cache, bp);
  }
  
  // enter new_block into final indirection block  
  bp = get_block(cache, block, BLK_READ);    
  write_indirect_block_entry(bp, offs[depth], new_block);
  block_markdirty(bp);
  put_block(cache, bp);
  inode->i_blocks += sb_sectors_in_block;
  
  return 0;
}


/* @brief   Delete an entry from an inode's block map
 *
 * @param   inode,
 * @param   position,
 * @return  0 on success, negative errno on error
 *
 * This does not free the file block itself, only the entry
 * in the block map and the indirect blocks that are now empty
 * due to deleting the entry from the map.
 *
 * TODO: We may want add option to delete mulitple/all blocks
 * to make file deletion or truncation faster.
 *
 * TODO: Need to work out way of optimizing deletes, currently writing
 * NO_BLOCK entry and clearing single bit in bitmap for each block
 * before flushing each time. 
 */
int delete_map_entry(struct inode *inode, off_t position)
{
  uint32_t block_pos;
  uint32_t indirect_blocks[4];
  uint32_t offs[4];
  int depth;
  int actual_depth;
  bool last_empty;
  struct buf *bp;
  
  block_pos = position / sb_block_size;

  depth = calc_block_indirection_offsets(block_pos, offs);

  if (depth < 0) {
    return -EINVAL;
  }
  
  inode_markdirty(inode);

  if (depth == 0) {
    inode->i_block[offs[0]] = NO_BLOCK;
    inode->i_blocks -= sb_sectors_in_block;
    return 0;    
  }

  // Descend the indirect blocks, reading each indirect block and
  // store pointers to the cached buffers in the indirect_buf array.
  
  actual_depth = get_indirect_blocks(inode, depth, offs, indirect_blocks);

  if (actual_depth < 0) {
    return -EINVAL;
  } else if (actual_depth == 0) {
    return 0;
  }

  // If the number of indirect blocks is correct we can delete the
  // entry from the final indirect block.
  
  if (actual_depth == depth) {
    bp = get_block(cache, indirect_blocks[depth], BLK_READ);

    if (bp == NULL) {
      panic("extfs: Cannot get indirect block");
    }

    write_indirect_block_entry(bp, offs[depth], NO_BLOCK);
    block_markdirty(bp);
    put_block(cache, bp);
    inode->i_blocks -= sb_sectors_in_block;
  }
  
  // Ascend the indirect blocks, check if it is empty, if so, mark it
  // as empty in the parent indirect block and free the indirect block  
  last_empty = false;
  
  for (int t = actual_depth; t >= 1; t--) {    
    bp = get_block(cache, indirect_blocks[t], BLK_READ);
    
    if (bp == NULL) {
      panic("extfs: Cannot get indirect block");
    }
    
    if (last_empty == true) {
      write_indirect_block_entry(bp, offs[t], NO_BLOCK);
      block_markdirty(bp);
      
      // FIXME: Really need to write bp indirect block before freeing [t+1] block below      
      // otherwise underlying block is freed, we crash, but parent indirect block still
      // points to it.
      free_block(indirect_blocks[t+1]);
      inode->i_blocks -= sb_sectors_in_block;
    }
    
    if (is_empty_indirect_block(bp)) {
      last_empty = true;
    } else {
      last_empty = false;
    }
    
    put_block(cache, bp);
  }

  if (last_empty == true) {
    set_toplevel_indirect_block_entry(inode, depth, NO_BLOCK);
    // FIXME: Really need to write inode before freeing indirect_blocks[1] block below      
    free_block(indirect_blocks[1]);    
    inode->i_blocks -= sb_sectors_in_block;    
  }        

  return 0;
}
  

/* @brief   Calculate block indirection offsets
 *
 * @param   position, byte position within a file
 * @param   array to store the offsets of the direct, single indirect,
            double indirect and triple indirect blocks
 * @return  depth of indirect blocks for this file position
 */
int calc_block_indirection_offsets(uint32_t position, uint32_t offs[4])
{
  uint32_t block_pos = position / sb_block_size;
	int depth;

  if (block_pos >= sb_out_range_s) {
  	return -EINVAL;
  	
  } else if (block_pos < EXT2_NDIR_BLOCKS) {
    // direct block
		offs[0] = block_pos;		
  	depth = 0;
  	
  } else if (block_pos < sb_doub_ind_s) {
    // single indirect block
    offs[0] = EXT2_IND_BLOCK;    
		offs[1] = (block_pos - EXT2_NDIR_BLOCKS);		
  	depth = 1;
    	  
  } else if (block_pos < sb_triple_ind_s) {
    // double indirect block
    offs[0] = EXT2_DIND_BLOCK;
		offs[1] = (block_pos - sb_doub_ind_s) / sb_addr_in_block;
		offs[2] = (block_pos - sb_doub_ind_s) % sb_addr_in_block;
    depth = 2;
    
  } else {
    // triple indirect block
    offs[0] = EXT2_TIND_BLOCK;
		offs[1] = (block_pos - sb_triple_ind_s) / sb_addr_in_block2;
		offs[2] = ((block_pos - sb_triple_ind_s) % sb_addr_in_block2) / sb_addr_in_block;
		offs[3] = ((block_pos - sb_triple_ind_s) % sb_addr_in_block2) % sb_addr_in_block;
    depth = 3;
  }
  
  return depth;
}


/* @brief   Get the block numbers of indirect blocks
 *
 * @param
 * @param
 * @param
 * @param
 * @return
 */
int get_indirect_blocks(struct inode *inode, int depth, uint32_t offs[4], uint32_t block[4])
{
  int actual;
  struct buf *bp;
  
  block[9] = 0;   // dummy value to align offs[x] and block[x]
  
  for (actual = 0; actual < depth; actual++) {
    if (actual == 0) {
      block[1] = get_toplevel_indirect_block_entry(inode, depth);

    } else {
      bp = get_block(cache, block[actual], BLK_READ);
      block[actual+1] = read_indirect_block_entry(bp, offs[actual]);
      put_block(cache, bp);
    }
    
    if (block[actual+1] == NO_BLOCK) {
      break;
    }
  }
  
  return actual;
}


/* @brief
 *
 * @param
 * @param
 * @return
 */
uint32_t get_toplevel_indirect_block_entry(struct inode *inode, int depth)
{
  uint32_t block;
  
  if (depth == 1) {
    block = inode->i_block[EXT2_IND_BLOCK];
  } else if (depth == 2) {
    block = inode->i_block[EXT2_DIND_BLOCK];
  } else if (depth == 3) {
    block = inode->i_block[EXT2_TIND_BLOCK];
  } else {
    panic("extfs: invalid indirect block depth: %d", depth);
  }

  return block;
}


/* @brief
 *
 * @param
 * @param
 * @param
 */
void set_toplevel_indirect_block_entry(struct inode *inode, int depth, uint32_t block)
{
  if (depth == 1) {
    inode->i_block[EXT2_IND_BLOCK] = block;
  } else if (depth == 2) {
    inode->i_block[EXT2_DIND_BLOCK] = block;
  } else if (depth == 3) {
    inode->i_block[EXT2_TIND_BLOCK] = block;
  } else {
    panic("extfs: invalid indirect block depth: %d", depth);
  }
}

  
/* @brief   Read an entry from an indirect block
 *
 * @param   bp, pointer to indirect block
 * @param   index, index into indirect block which contains the block number
 * @return  block number read at index location
 */
block_t read_indirect_block_entry(struct buf *bp, int index)
{
  if (bp == NULL) {
  	panic("extfs: read_indirect_block buf is NULL");
  }

  return bswap4(be_cpu, ((uint32_t *)bp->data)[index]);
}


/* @brief   Write an entry to an indirect block
 *
 * @param   bp, pointer to indirect block
 * @param   index, index into indirect block in which to write new block number
 * @param   block, block number to write 
 */
void write_indirect_block_entry(struct buf *bp, int index, block_t block)
{
  if(bp == NULL) {
  	panic("write_indirect_block() on NULL");
  }
  
  /* write a block into an indirect block */ 
  ((uint32_t *)bp->data)[index] = bswap4(be_cpu, block);
}


/* @brief   Check if the indirect block contains no entries
 *
 * @brief   bp, pointer to indirect block 
 * @return  true if the indirect block is empty, false otherwise.
 */
bool is_empty_indirect_block(struct buf *bp)
{
  for (uint32_t i = 0; i < sb_addr_in_block; i++) {
  	if (((uint32_t *)bp->data)[i] != NO_BLOCK) {
  		return false;
    }
  }
  
  return true;
}


/* @brief   Fill a block in the block cache with zeroes
 *
 * @param   buf returned by a call to get_block/
 */
void zero_block(struct buf *bp)
{
  memset(bp->data, 0, sb_block_size);
  block_markdirty(bp);
}


/* @brief   Allocate a block for inode.
 *
 * @param   inode, if block is NO_BLOCK then allocate block where this inode is
 * @param   block, if not NO_BLOCK then use as a goal block to allocate near
 * @return  block number of allocated block or NO_BLOCK on error
 */
block_t alloc_block(struct inode *inode, block_t goal)
{
  struct buf *bp;
  struct group_desc *gd;
  block_t block = NO_BLOCK;
  uint32_t bit = NO_BLOCK;
  uint32_t *bitmap;
  int group;
  int word;                 /* word in block bitmap to start searching from */

  if (superblock.s_free_blocks_count == 0) {
  	return NO_BLOCK;
  }

  if (goal == NO_BLOCK) {
	  group = (inode->i_ino - 1) / superblock.s_inodes_per_group;
	  goal = superblock.s_blocks_per_group * group + superblock.s_first_data_block;
  }

  if (goal >= superblock.s_blocks_count || (goal < superblock.s_first_data_block && goal != NO_BLOCK)) {
  	goal = rand() % superblock.s_blocks_count;
  }

  /* Allocate a block starting from the goal's group and from the goal's bitmap "word".
   * We wrap around to the first group and continue searching.  We finally search the
   * goal's group again, but this time from beginning of goal group's bitmap.
   */
  group = (goal - superblock.s_first_data_block) / superblock.s_blocks_per_group;
  word = ((goal - superblock.s_first_data_block) % superblock.s_blocks_per_group) / 32;

  for (int i = 0; i <= sb_groups_count; i++) {
	  group = i % sb_groups_count;
    
	  gd = get_group_desc(group);

	  if (gd == NULL) {
		  panic("extfs: can't get group_desc to alloc block");
    }
    
	  if (gd->g_free_blocks_count == 0) {
		  word = 0;
		  continue;
	  }

	  bp = get_block(cache, gd->g_block_bitmap, BLK_READ);
    
    if (bp == NULL) {
      panic("extfs: failed to get bitmap block for alloc block");
    }
    
    bitmap = (uint32_t *)bp->data;
    
    bit = alloc_bit(bitmap, superblock.s_blocks_per_group, word);

    if (bit != NO_BLOCK) {
	    block = superblock.s_first_data_block + (group * superblock.s_blocks_per_group) + bit;

	    check_block_number(gd, block);

	    block_markdirty(bp);
	    put_block(cache, bp);

	    gd->g_free_blocks_count--;
	    superblock.s_free_blocks_count--;

      group_descriptors_markdirty();
      return block;
    } 
    
    /* For first block we search from goal word in bitmap, so there may be free
     * bits before the 'goal' bit.  For all other blocks and also next time we
     * check the 'goal' block we start from the beginning of the bitmap. If we
     * fail to allocate a bit but g_free_blocks_count says the group has free
     * bits then we panic.
     */
    if (i != 0) {
		  panic("extfs: allocator failed to allocate a bit in bitmap with free bits");
	  }
		
		word = 0;
  }
  
  return NO_BLOCK;
}


/* @brief   Free a block
 * 
 * @param   block, the block to free
 */
void free_block(block_t block)
{
  int group;		        
  int bit;
  struct buf *bp;
  struct group_desc *gd;
  uint32_t *bitmap;

  if (block >= superblock.s_blocks_count || block < superblock.s_first_data_block) {
	  panic("extfs: trying to free block %d beyond blocks scope.", block);
  }

  group = (block - superblock.s_first_data_block) / superblock.s_blocks_per_group;
  bit = (block - superblock.s_first_data_block) % superblock.s_blocks_per_group;
  
  if ((gd = get_group_desc(group)) == NULL) {
  	panic("extfs: can't get group_desc to alloc block");
  }
  
  check_block_number(gd, block);
  
  bp = get_block(cache, gd->g_block_bitmap, BLK_READ);

  bitmap = (uint32_t *)bp->data;

  if (clear_bit(bitmap, bit)) {
	  panic("extfs: failed freeing unused block %d", block);
  }
  
  block_markdirty(bp);
  put_block(cache, bp);

  gd->g_free_blocks_count++;
  superblock.s_free_blocks_count++;

  group_descriptors_markdirty();
  
  invalidate_block(cache, block);
}


/* @brief   Sanity checking to ensure allocated block is not a system block
 *
 * @brief   gd, group descriptor that block should belong to
 * @brief   block, block number to check 
 */
void check_block_number(struct group_desc *gd, block_t block)
{
  if (block == gd->g_inode_bitmap || block == gd->g_block_bitmap ||
      !(block >= gd->g_inode_table && block < (gd->g_inode_table + sb_inode_table_blocks_per_group))) {	
	  panic("extfs: block allocator tried to return a system block");
  }

  if (block >= superblock.s_blocks_count) {
	  panic("extfs: block allocator returned block number greater than total number of blocks");
  }
}



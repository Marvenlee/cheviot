/* This file manages inodes allocation and deallocation.
 *
 * Created (alloc_inode/free_inode/wipe_inode are from MFS):
 *   June 2010 (Evgeniy Ivanov)
 * 
 * Updated (CheviotOS Filesystem Handler based)
 *   December 2023 (Marven Gilhespie) 
 */

#define LOG_LEVEL_ERROR

#include "ext2.h"
#include "globals.h"


/* @brief   Create a new inode and a new directory entry
 *
 * @param   dir_inode, parent directory inode that new inode will belong to
 * @param   name, filename on new inode
 * @param   mode, mode permission and file type bits
 * @param   uid, user ID the new inode belongs to
 * @param   gid, group ID the new inode belongs to
 * @param   res, returned pointer to the new inode
 * @return  0 on success, negative errno on failure
 */
int new_inode(struct inode *dir_inode, char *name, mode_t mode, 
                        uid_t uid, gid_t gid, struct inode **res)
{
  struct inode *inode;
  int sc;
  ino_t ino_nr;
  
  *res = NULL;
  
  if (dir_inode->i_links_count == 0) {
  	return -ENOENT;
  }

  // If creating directory, will need a link for ".." in new directory
  if (S_ISDIR(mode) && dir_inode->i_links_count >= LINK_MAX) {
    return -EMLINK;
  }

  if (lookup_dir(dir_inode, name, &ino_nr) == 0) {
    return -EEXIST;
  }
  
  if ((sc = alloc_inode(dir_inode, mode, uid, gid, &inode)) != 0) {	  
	  return sc;
  }

  // Flush the inode prior to adding a dirent to the directory
  inode->i_links_count++;
  write_inode(inode);

  if ((sc = dirent_enter(dir_inode, name, inode->i_ino, mode)) != 0) {
	  inode->i_links_count--;
	  inode_markdirty(inode);
	  put_inode(inode);
	  return sc;
  }
	  
	*res = inode;
  return 0;
}


/* @brief   Allocate an inode
 *
 * @param   parent_inode, allocate an inode close to it's parent inode
 * @param   mode, file mode (permissions and type) of new inode
 * @param   uid, user id of new inode
 * @param   gid, group id of new inode
 * @param   res, returned pointer to newly allocated inode
 * @return  0 on success, negative errno on failure
 */
int alloc_inode(struct inode *parent_inode, mode_t mode, uid_t uid, gid_t gid, struct inode **res)
{
  struct inode *inode;
  ino_t ino_nr;
  uint32_t group;
  
	*res = NULL;

	if (S_ISDIR(mode)) {
		group = find_free_inode_dir_group(parent_inode->i_ino);
	} else {
		group = find_free_inode_file_group(parent_inode->i_ino);
  }

  if (group == NO_GROUP) {
  	return -ENOSPC;
  }

  ino_nr = alloc_inode_bit(group, S_ISDIR(mode));

  if (ino_nr == NO_INODE) {
    return -ENOSPC;
  }

  // FIXME: Avoid reading in inode with get_inode, add get_new_inode
  if ((inode = get_inode(ino_nr)) == NULL) {
  	free_inode_bit(ino_nr, S_ISDIR(mode));
    return -EIO;
  }
  
  inode->i_mode = mode;
  inode->i_links_count = 0;
  inode->i_uid = uid;
  inode->i_gid = gid;
  inode->i_size = 0;
  inode->i_update = ATIME | CTIME | MTIME;
  inode->i_blocks = 0;
  inode->i_flags = 0;
  inode->i_generation = 0;
  inode->i_file_acl = 0;
  inode->i_dir_acl = 0;
  inode->i_faddr = 0;

  for (int i = 0; i < EXT2_N_BLOCKS; i++) {
    inode->i_block[i] = NO_BLOCK;
  }

  inode_markdirty(inode);
	*res = inode;
	return 0;
}


/* @brief   Free an inode
 * 
 * @param   inode, pointer to inode to free
 */
void free_inode(struct inode *inode)
{
  uint32_t b = inode->i_ino;

  if (b <= NO_ENTRY || b > superblock.s_inodes_count) {
	  log_warn("extfs: freeing block that is out of range, ignoring");
	  return;
  }
  
  free_inode_bit(b, S_ISDIR(inode->i_mode));
  inode->i_mode = 0;  
}


/* @brief   Allocate a bit in the inode bitmap
 *
 * @param   parent_inode, parent of newly allocated inode 
 * @param   is_dir, if true, update dirs counters
 * @return  inode number allocated or NO_INODE if no free inodes
 */
uint32_t alloc_inode_bit(uint32_t group, bool is_dir)
{
  ino_t ino_nr = NO_INODE;
  uint32_t bit;
  struct buf *bp;
  struct group_desc *gd;
  uint32_t *bitmap;
    
  gd = get_group_desc(group);

  if (gd == NULL) {
	  panic("extfs: can't get group_desc to alloc block");
  }
  
  if (gd->g_free_inodes_count == 0) {
    panic("extfs: group desc reports no free inodes but earlier search reported it does");
  }
  
  bp = get_block(cache, gd->g_inode_bitmap, BLK_READ);
  
  bitmap = (uint32_t *)bp->data;  
  bit = alloc_bit(bitmap, superblock.s_inodes_per_group, 0);

  if (bit == NO_BLOCK) {
    panic("extfs: unable to alloc bit in bitmap, but descriptor indicated free blocks"); 
  }
  
  ino_nr = group * superblock.s_inodes_per_group + bit + 1;

  if (ino_nr > superblock.s_inodes_count) {
	  panic("extfs: allocator returned inode number greater than total inodes");
  }

  if (ino_nr < sb_first_ino) {
  	panic("extfs: allocator tried to return reserved inode");
  }

  block_markdirty(bp);
  put_block(cache, bp);

  gd->g_free_inodes_count--;
  superblock.s_free_inodes_count--;

  if (is_dir) {
	  gd->g_used_dirs_count++;
	  sb_dirs_counter++;
  }

  group_descriptors_markdirty();

  return ino_nr;
}


/* @brief   Free a bit in the inode bitmap
 *
 * @param   ino_nr, inode number corresponding to bit in bitmap to free
 * @param   is_dir, if true update dirs counters
 */
void free_inode_bit(uint32_t ino_nr, bool is_dir)
{
  int group;
  uint32_t bit;
  struct buf *bp;
  struct group_desc *gd;
  uint32_t *bitmap;
  
  if (ino_nr > superblock.s_inodes_count || ino_nr < sb_first_ino) {
	  panic("trying to free inode %d beyond inodes scope.", ino_nr);
  }
  
  group = (ino_nr - 1) / superblock.s_inodes_per_group;
  bit = (ino_nr - 1) % superblock.s_inodes_per_group; /* index in bitmap */

  gd = get_group_desc(group);
  
  if (gd == NULL) {
	  panic("can't get group_desc to alloc block");
  }
  
  bp = get_block(cache, gd->g_inode_bitmap, BLK_READ);

  bitmap = (uint32_t *)bp->data;
  if (clear_bit(bitmap, ino_nr)) {
	  panic("Tried to free unused inode %d", ino_nr);
  }
  
  block_markdirty(bp);
  put_block(cache, bp);

  gd->g_free_inodes_count++;
  superblock.s_free_inodes_count++;

  if (is_dir) {
    gd->g_used_dirs_count--;
    sb_dirs_counter--;
  }

  group_descriptors_markdirty();
}


/* @brief   Find the best group to create a new directory inode in
 *
 * @param   parent_ino, inode number of parent directory
 * @return  Group containing free inodes or NO_GROUP
 *
 * Performs a search similar to find_group_dir in Linux
 */
uint32_t find_free_inode_dir_group(uint32_t parent_ino)
{
  struct group_desc *gd;
  struct group_desc *best_gd = NULL;
  int avg_free_inodes_per_group;
  uint32_t parent_group;
  uint32_t group;
  uint32_t best_group = NO_GROUP;
  
  avg_free_inodes_per_group = superblock.s_free_inodes_count / sb_groups_count;   

	if (parent_ino == EXT2_ROOT_INO) {  
		parent_group = rand() % sb_groups_count;  // Perhaps limit to first few groups?
  } else {
		parent_group = (parent_ino - 1) / superblock.s_inodes_per_group;
  }
   
	for (uint32_t t = 0; t < sb_groups_count; t++) {
		group = (parent_group + t) % sb_groups_count;

    gd = get_group_desc(group);
    
    if (gd == NULL) {
      panic("extfs: Can't get group_desc to alloc inode");
    }
        
    if (gd->g_free_inodes_count == 0 || gd->g_free_inodes_count < avg_free_inodes_per_group) {
      continue;
    }
        
    if (best_gd == NULL || gd->g_free_blocks_count > best_gd->g_free_blocks_count) {
      best_gd = gd;
      best_group = group;
    }    
  }
  
  return best_group;
}


/* @brief   Find the best group to create a new file inode in
 *
 * @param   parent_ino, inode number of parent directory
 * @return  Group containing free inodes or NO_GROUP
 *
 * Similar to BSD ffs_hashalloc()
 * 1) Check parent group for free inodes and blocks
 * 2) if no free group found, quadradically search on the group number
 * 3) if no free group found, linear search for free inode
 */
uint32_t find_free_inode_file_group(uint32_t parent_ino)
{
  struct group_desc *gd;
  uint32_t parent_group = (parent_ino - 1) / superblock.s_inodes_per_group;
	uint32_t group = (parent_group + parent_ino) % sb_groups_count;
	uint32_t i;
	
  gd = get_group_desc(parent_group);

	if (gd->g_free_inodes_count && gd->g_free_blocks_count) {
		return parent_group;
  }
    
	for (uint32_t t = 1; t < sb_groups_count; t <<= 1) {
		group = (group + t) % sb_groups_count;
    gd = get_group_desc(parent_group);
    
	  if (gd->g_free_inodes_count && gd->g_free_blocks_count) {
		  return group;
    }
	}

  group = parent_group;
	
	for (uint32_t t = 0; t < sb_groups_count; t++) {
		group = (group + 1) % sb_groups_count;

    gd = get_group_desc(parent_group);
    
	  if (gd->g_free_inodes_count) {
		  return group;
    }
	}

	return NO_GROUP;
}


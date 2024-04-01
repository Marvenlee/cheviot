/* The dir_delete.c, dir_enter.c, dir_isempty.c and dir_lookup.c are partially
 * based on the Minix Ext2 FS path.c fs_lookup function but split into separate
 * operations for clarity. They contain the procedures for looking up path names
 * and performing operations on directory entries.
 *
 * Created (MFS based):
 *   February 2010 (Evgeniy Ivanov)
 *
 * Updated (CheviotOS Filesystem Handler based)
 *   December 2023 (Marven Gilhespie) 
 */

#define LOG_LEVEL_INFO

#include "ext2.h"
#include "globals.h"


/* @brief   Delete an dirent within a directory
 *
 * @param   dir_inode, inode of directory to search within
 * @param   name, name of item in the directory to delete
 * @return  0 on success, negative errno on failure
 */
int dirent_delete(struct inode *dir_inode, char *name)
{
  struct dir_entry *dp = NULL;
  struct dir_entry *prev_dp = NULL;
  struct buf *bp = NULL;
  off_t pos = 0;
  int string_len = 0;
  int r = 0;
    
  if ((string_len = strlen(name)) > EXT2_NAME_MAX) {
	  return -ENAMETOOLONG;
  }
  
  while(pos < dir_inode->odi.i_size) {
	  if(!(bp = get_dir_block(dir_inode, pos))) {
		  panic("dirent_delete found a hole in a directory");
    }
    
    r = search_block_and_delete(dir_inode, bp, name);

    if (r == 0) {   // file dirent has been deleted
      put_block(cache, bp);
      return 0;
    }

	  put_block(cache, bp);
    pos += sb_block_size;
  }

  /* The whole directory has now been searched. */
  
  return -ENOENT;
}


/* @brief   Search a directory block for the dirent and delete if found
 *
 * @param   dir_inode, inode of directory to search within
 * @param   bp, pointer to current cache block to search
 * @param   name, name of file
 * @return  0 if dirent is found and deleted, negative errno on failure.
 */
int search_block_and_delete(struct inode *dir_inode, struct buf *bp, char *name)
{
	struct dir_entry *prev_dp;
  struct dir_entry *dp;
  
	prev_dp = NULL;  
  dp = (struct dir_entry*) bp->data;
  
  while(CUR_DISC_DIR_POS(dp, bp->data) < sb_block_size) {	  
	  if (dp->d_ino != NO_ENTRY) {
	    if (strcmp_nz(dp->d_name, name, dp->d_name_len) == 0) {
        delete_dir_entry(dir_inode, dp, prev_dp, bp);
        return 0;
	    }
    }
            
	  prev_dp = dp;
	  dp = NEXT_DISC_DIR_DESC(dp);
  }

  return -ENOENT;
}


/* @brief   Delete a dirent from a directory block
 *
 * @param   dir_inode, inode of directory to delete dirent from
 * @param   dp, pointer to dirent to be deleted
 * @param   prev_dp, pointer to previous dirent if any in the block
 * @param   bp, pointer to cached block in which the dirent is to be removed
 * @param   pos, offset to look from on next dirent "enter"
 */
void delete_dir_entry(struct inode *dir_inode, struct dir_entry *dp, 
                      struct dir_entry *prev_dp, struct buf *bp)
{
	/* if space available, Save d_ino for recovery. */
  if (dp->d_name_len >= sizeof(ino_t)) {
	  size_t t = dp->d_name_len - sizeof(ino_t);
	  memcpy(&dp->d_name[t], &dp->d_ino, sizeof(dp->d_ino));
  }
  
  dp->d_ino = NO_ENTRY;
  block_markdirty(bp);

  dir_inode->odi.i_flags &= ~EXT2_INDEX_FL;  
  dir_inode->i_update |= CTIME | MTIME;
  inode_markdirty(dir_inode);

  /* Merge with previous dirent if any. The following dirent if it was
   * already free will havw been merged with the current one previously. */
  if (prev_dp) {
	  uint16_t temp = bswap2(be_cpu, prev_dp->d_rec_len);
	  temp += bswap2(be_cpu, dp->d_rec_len);
	  prev_dp->d_rec_len = bswap2(be_cpu, temp);
  }
  
  write_inode(dir_inode);
}



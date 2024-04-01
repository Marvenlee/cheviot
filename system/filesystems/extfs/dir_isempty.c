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


/* @brief   Check if a directory is empty
 *
 * @param   dir_inode, inode of directory to check if empty
 * @return  true if directory is empty, false otherwise
 */
bool is_dir_empty(struct inode *dir_inode)
{
  struct buf *bp = NULL;
  off_t pos = 0;
  int r = 0;
  
  while(pos < dir_inode->odi.i_size) {
	  if(!(bp = get_dir_block(dir_inode, pos))) {
		  panic("extfs: is_dir_empty found a hole in a directory");
    }
    
    if (is_dir_block_empty(bp) == false) {
      put_block(cache, bp);
      return false;
    }
	  
	  put_block(cache, bp);
    pos += sb_block_size;
  }

  return true;
}


/* @brief   Search a directory block.
 *
 * @param   bp, pointer to a cached directory block
 * @return  true if block has no files, false otherwise
 */
bool is_dir_block_empty(struct buf *bp)
{
  struct dir_entry *dp;
    
  dp = (struct dir_entry*) bp->data;
  
  while(CUR_DISC_DIR_POS(dp, bp->data) < sb_block_size) {	  
    if (dp->d_ino != NO_ENTRY) {
	    if (strcmp_nz(dp->d_name, ".", dp->d_name_len) != 0 &&
	        strcmp_nz(dp->d_name, "..", dp->d_name_len) != 0) {
  	    /* not empty, dir contains something other than "." and ".." */
	      return false;
      }
    }

	  dp = NEXT_DISC_DIR_DESC(dp);
  }

  return true;
}


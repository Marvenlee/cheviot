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


/* @brief   Search a directory for a matching filename
 *
 * @param   dir_inode, inode of directory to search within
 * @param   name, name of item in the directory to find or create
 * @param   ino_nr, inode number returned by lookup
 * @return  0 on success, negative errno on failure
 * 
 */
int lookup_dir(struct inode *dir_inode, char *name, ino_t *ino_nr)
{
  struct buf *bp = NULL;
  off_t pos = 0;
  int string_len = 0;
  int r = 0;
    
  if ((string_len = strlen(name)) > EXT2_NAME_MAX) {
	  return -ENAMETOOLONG;
  }
  
  while(pos < dir_inode->odi.i_size) {
	  if(!(bp = get_dir_block(dir_inode, pos))) {
		  panic("lookup_dir found a hole in a directory");
    }
    
    r = lookup_dir_block(dir_inode, bp, name, ino_nr);

    if (r == 0) {   // inode number is returned in ino_nr
      put_block(cache, bp);
      return 0;
    }

	  put_block(cache, bp);
    pos += sb_block_size;
  }

  return -ENOENT;
}


/* @brief   Search a directory block.
 *
 * @param   dir_inode,  
 * @param   bp,
 * @param   name,
 * @param   ret_ino_nr,
 * @return
 */ 
int lookup_dir_block(struct inode *dir_inode, struct buf *bp,
                     char *name, ino_t *ret_ino_nr)
{
  struct dir_entry *dp = (struct dir_entry*) bp->data;
  
  while(CUR_DISC_DIR_POS(dp, bp->data) < sb_block_size) {	  
    if (dp->d_ino != NO_ENTRY) {
	    if (strcmp_nz(dp->d_name, name, dp->d_name_len) == 0) {
	      *ret_ino_nr = (ino_t) bswap4(be_cpu, dp->d_ino);
	      return 0;
	    }
    }

	  dp = NEXT_DISC_DIR_DESC(dp);
  }

  return -ENOENT;
}


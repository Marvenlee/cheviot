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


/* @brief   Add a new entry to a directory with with name, inode ino_nrber and type
 *
 * @param   dir_inode, inode of directory to search within
 * @param   name, name of new dirent 
 * @param   ino_nr, inode number of new dirent
 * @param   ftype, file type to set in new dirent.
 * @return  0 on success, negative errno on failure
 */
int dirent_enter(struct inode *dir_inode, char *name, ino_t ino_nr, mode_t mode)
{
  struct dir_entry *dp = NULL;
  struct dir_entry *prev_dp = NULL;
  struct buf *bp = NULL;
  off_t pos = 0;
  int required_space = 0;
  int name_len = 0;
  int r = 0;
  
  
  if ((name_len = strlen(name)) > EXT2_NAME_MAX) {
	  return -ENAMETOOLONG;
  }
  
  required_space = MIN_DIR_ENTRY_SIZE + name_len;
  required_space += ((required_space & 0x03) == 0) ? 0 : (DIR_ENTRY_ALIGN - (required_space & 0x03) );

  while(pos < dir_inode->i_size) {
	  if(!(bp = get_dir_block(dir_inode, pos))) {
		  panic("dirent_enter found a hole in a directory");
    }
    
    r = find_dirent_free_space(dir_inode, bp, required_space, &dp);

	  if (r == 0) {
      return enter_dirent(dir_inode, bp, dp, ino_nr, name, name_len, mode);
	  }

	  put_block(cache, bp);
    pos += sb_block_size;
  }

  /* The whole directory has now been searched. */  
  return -ENOENT;
}


/* @brief   Search a directory block for free space for a new dirent
 *
 * @param   dir_inode,  
 * @param   bp,
 * @param   required_space,
 * @param   ret_dp
 * @return
 */
int find_dirent_free_space(struct inode *dir_inode, struct buf *bp,
                           size_t required_space, struct dir_entry **ret_dp)
{
	struct dir_entry *prev_dp;
  struct dir_entry *dp;
  bool match = false;
  size_t available_size_if_shrunk;
    
	prev_dp = NULL; /* New block - new first dentry, so no prev. */
  
  dp = (struct dir_entry*) bp->data;
  
  while(CUR_DISC_DIR_POS(dp, bp->data) < sb_block_size) {	  
    if (dp->d_ino == NO_ENTRY) {
	    if (required_space <= bswap2(be_cpu, dp->d_rec_len)) {
		    *ret_dp = dp;
		    return 0;
	    }
	  }

    available_size_if_shrunk = bswap2(be_cpu, (dp)->d_rec_len) - DIR_ENTRY_ACTUAL_SIZE(dp);

    if (required_space <= available_size_if_shrunk) {
      *ret_dp = shrink_dir_entry(dp, bp);
	    return 0;
	  }

	  prev_dp = dp;
	  dp = NEXT_DISC_DIR_DESC(dp);
  }

  return -ENOSPC;
}


/* @brief   Enter a new entry in a directory
 *
 * @param   dir_inode,  
 * @param   bp,
 * @param   dp
 * @param   ino_nr
 * @param   name
 * @param   name_len
 * @param   ftype
 * @return
 */
int enter_dirent(struct inode *dir_inode, struct buf *bp, struct dir_entry *dp,
                 ino_t ino_nr, char *name, size_t name_len, mode_t mode)
{
  bool extended = false;
  
  if (dp == NULL) { /* No free space was found in previous search so extend directory */
    if ((dp = extend_directory(dir_inode, &bp)) == NULL) {
      return -ENOMEM;
    }

	  extended = true;
  }

  dp->d_ino = (int) bswap4(be_cpu, ino_nr);
  dp->d_name_len = name_len;
  set_dirent_file_type(dp, mode);
  
  for (int i = 0; i < NAME_MAX && i < dp->d_name_len && name[i] != '\0'; i++) {
	  dp->d_name[i] = name[i];
  }
  	
  block_markdirty(bp);
  put_block(cache, bp);

  if (extended) {
  	dir_inode->i_size += (off_t) bswap2(be_cpu, dp->d_rec_len);
  }

  dir_inode->i_update |= CTIME | MTIME;
  inode_markdirty(dir_inode);
  write_inode(dir_inode);

  return 0;
}


/* @brief   Extend a directory with a new block and dirent
 *
 * @param   dir_inode
 * @param   bpp
 * @return
 */
struct dir_entry *extend_directory(struct inode *dir_inode, struct buf **bpp)
{    
  struct dir_entry *dp;
  
  if ((*bpp = new_block(dir_inode, dir_inode->i_size)) == NULL) {
	  return NULL;
	}
	  
  dp = (struct dir_entry *)((*bpp)->data);
  dp->d_rec_len = bswap2(be_cpu, sb_block_size);
  dp->d_name_len = DIR_ENTRY_MAX_NAME_LEN(dp); /* for failure */
  return dp;
}


/* @brief   Shrink a directory entry
 *
 * @param   dp,
 * @param   bp,
 * @return
 */
struct dir_entry *shrink_dir_entry(struct dir_entry *dp, struct buf *bp)
{
  int new_slot_size = bswap2(be_cpu, dp->d_rec_len);
  int actual_size = DIR_ENTRY_ACTUAL_SIZE(dp);
  new_slot_size -= actual_size;
  dp->d_rec_len = bswap2(be_cpu, actual_size);
  dp = NEXT_DISC_DIR_DESC(dp);
  dp->d_rec_len = bswap2(be_cpu, new_slot_size);
  dp->d_ino = NO_ENTRY;
  block_markdirty(bp);
  return dp;
}		    


/* @brief   Set a dir_entry's file type
 *
 * @param   dp, pointer to dir_entry
 * @param   ftype, file type from mode bits
 */
void set_dirent_file_type(struct dir_entry *dp, mode_t mode)
{
  if (HAS_INCOMPAT_FEATURE(&superblock, EXT2_FEATURE_INCOMPAT_FILETYPE)) {
	  if (S_ISREG(mode))
		  dp->d_file_type = EXT2_FT_REG_FILE;
	  else if (S_ISDIR(mode))
		  dp->d_file_type = EXT2_FT_DIR;
	  else if (S_ISLNK(mode))
		  dp->d_file_type = EXT2_FT_SYMLINK;
	  else if (S_ISBLK(mode))
		  dp->d_file_type = EXT2_FT_BLKDEV;
	  else if (S_ISCHR(mode))
		  dp->d_file_type = EXT2_FT_CHRDEV;
	  else if (S_ISFIFO(mode))
		  dp->d_file_type = EXT2_FT_FIFO;
	  else
		  dp->d_file_type = EXT2_FT_UNKNOWN;
  }
}

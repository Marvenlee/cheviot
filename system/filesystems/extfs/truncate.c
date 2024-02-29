/* This file handles truncating of files.
 *
 * Created (CheviotOS Filesystem Handler based)
 *   December 2023 (Marven Gilhespie) 
 */

#define LOG_LEVEL_INFO

#include "ext2.h"
#include "globals.h"


/* @brief   Truncate the contents of a inode, freeing blocks
 *
 */
int truncate_inode(struct inode *inode, ssize_t sz)
{
  return -ENOSYS;
}

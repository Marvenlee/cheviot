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
  log_info("truncate_inode() ENOSYS ino_nr:%u, inode:%08x", (uint32_t)inode->i_ino, (uint32_t)inode);
  return -ENOSYS;  // FIXME: extfs truncate_inode
}

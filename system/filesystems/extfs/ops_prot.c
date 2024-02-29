/* This file handles messages for file protection operations.
 *
 * Created (CheviotOS Filesystem Handler based)
 *   December 2023 (Marven Gilhespie) 
 */

#define LOG_LEVEL_INFO

#include "ext2.h"
#include "globals.h"


/* @brief   Change the mode permission bits of a file or directory
 *
 * @param   fsreq, message header received by getmsg.
 */
void ext2_chmod(struct fsreq *req)
{
  struct inode *inode;
  
  inode = get_inode(req->args.chmod.inode_nr);
  
  if (inode == NULL) {
    replymsg(portid, msgid, -EIO, NULL, 0);
	  return;
  }

  inode->i_mode = req->args.chmod.mode;  
  inode->i_update |= CTIME;
  inode_markdirty(inode);
  put_inode(inode);

  replymsg(portid, msgid, 0, NULL, 0);
}


/* @brief   Change the owner and group of a file or directory
 * 
 * @param   fsreq, message header received by getmsg.
 */
void ext2_chown(struct fsreq *req)
{
  struct inode *inode;
  
  inode = get_inode(req->args.chown.inode_nr);
  
  if (inode == NULL) {
    replymsg(portid, msgid, -EIO, NULL, 0);
	  return;
  }
  
  inode->i_uid = req->args.chown.uid;
  inode->i_gid = req->args.chown.gid;
  inode->i_update |= CTIME;
  inode_markdirty(inode);
  put_inode(inode);

  replymsg(portid, msgid, 0, NULL, 0);
}


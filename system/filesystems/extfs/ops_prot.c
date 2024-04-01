/* This file handles messages for file protection operations.
 *
 * Created (CheviotOS Filesystem Handler based)
 *   December 2023 (Marven Gilhespie) 
 */

#define LOG_LEVEL_WARN

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
  	log_error("ext2_chmod: -ENOENT");
    replymsg(portid, msgid, -ENOENT, NULL, 0);
	  return;
  }

  // FIXME: Need suid bit handled (other bits too?)
  // TODO: Should be masked in kernel?
  inode->odi.i_mode = (inode->odi.i_mode & ~0777) | (req->args.chmod.mode & 0777);
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
  	log_error("ext2_chown: -ENOENT");
    replymsg(portid, msgid, -ENOENT, NULL, 0);
	  return;
  }
  
  inode->odi.i_uid = req->args.chown.uid;
  inode->odi.i_gid = req->args.chown.gid;
  inode->i_update |= CTIME;
  inode_markdirty(inode);
  put_inode(inode);

  replymsg(portid, msgid, 0, NULL, 0);
}


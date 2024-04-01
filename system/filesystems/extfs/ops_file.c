/* This file handles messages for directory operations.
 *
 * Created (CheviotOS Filesystem Handler based)
 *   December 2023 (Marven Gilhespie) 
 */

#define LOG_LEVEL_WARN

#include "ext2.h"
#include "globals.h"


/* @brief   Read a file
 *
 * @param   fsreq, message header received by getmsg.
 */
void ext2_read(struct fsreq *req)
{
  ssize_t nbytes_read;
  ino_t ino_nr;
  off_t offset;
  size_t count;
  int status;
  
  ino_nr = req->args.read.inode_nr;
  offset = req->args.read.offset;
  count = req->args.read.sz;
  
  nbytes_read = read_file(ino_nr, count, offset);
  replymsg(portid, msgid, nbytes_read, NULL, 0);
}


/* @brief   Write to a file
 *
 * @param   fsreq, message header received by getmsg.
 */
void ext2_write(struct fsreq *req)
{
  ssize_t nbytes_written;
  ino_t ino_nr;
  off_t offset;
  size_t count;

  ino_nr = req->args.write.inode_nr;
  offset = req->args.write.offset;
  count = req->args.write.sz;

  nbytes_written = write_file(ino_nr, count, offset);
  replymsg(portid, msgid, nbytes_written, NULL, 0);
}


/* @brief   Create a new file
 *
 * @param   fsreq, message header received by getmsg.
 */
void ext2_create(struct fsreq *req)
{
  struct fsreply reply = {0};
  struct inode *dir_inode;
  struct inode *inode;
  mode_t mode;
  uint32_t oflags;
  uid_t uid;
  gid_t gid;
  char name[NAME_MAX+1];
  int sc;
        
  readmsg(portid, msgid, name, req->args.create.name_sz, sizeof *req);
  
  if ((dir_inode = get_inode(req->args.create.dir_inode_nr)) == NULL) {
    replymsg(portid, msgid, -ENOENT, NULL, 0);
	  return;
  }

  oflags = req->args.create.oflags;
  
  // FIXME: ext2_create, assuming creation of regular files only
  mode = S_IFREG | (req->args.create.mode & 0777);
  uid = req->args.create.uid;
  gid = req->args.create.gid;
  
  sc = new_inode(dir_inode, name, mode, uid, gid, &inode);

  if (sc != 0) {
	  put_inode(dir_inode);
    replymsg(portid, msgid, sc, NULL, 0);
	  return;
  }

  reply.args.create.inode_nr = inode->i_ino;
  reply.args.create.mode = inode->odi.i_mode;
  reply.args.create.size = inode->odi.i_size;
  reply.args.create.uid = inode->odi.i_uid;
  reply.args.create.gid = inode->odi.i_gid;
  reply.args.create.atime = 0;
  reply.args.create.mtime = 0;
  reply.args.create.ctime = 0;
  
  put_inode(dir_inode);
  put_inode(inode);

  replymsg(portid, msgid, 0, &reply, sizeof reply);
} 
 

/* @brief   Truncate a file
 *
 * @param   fsreq, message header received by getmsg.
 */
void ext2_truncate(struct fsreq *req)
{  
  // TODO: FIXME:  sc = truncate_inode(struct inode *rip, off_t len)
  replymsg(portid, msgid, 0, NULL, 0);
}



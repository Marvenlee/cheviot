/*
 * Copyright 2023  Marven Gilhespie
 *
 * Licensed under the Apache License, segment_id 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_LEVEL_INFO

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
  struct fsreply reply;
  struct inode *dir_inode;
  struct inode *inode;
  mode_t mode;
  uint32_t oflags;
  uid_t uid;
  gid_t gid;
  char name[NAME_MAX+1];
  int sc;
  
  memset(&reply, 0, sizeof reply);
        
  readmsg(portid, msgid, name, req->args.create.name_sz, sizeof *req);
  
  if ((dir_inode = get_inode(req->args.create.dir_inode_nr)) == NULL) {
    replymsg(portid, msgid, -ENOENT, NULL, 0);
	  return;
  }

  oflags = req->args.create.oflags;
  mode = req->args.create.mode;
  uid = req->args.create.uid;
  gid = req->args.create.gid;
  
  sc = new_inode(dir_inode, name, mode, uid, gid, &inode);

  if (sc != 0) {
	  put_inode(dir_inode);
    replymsg(portid, msgid, sc, NULL, 0);
	  return;
  }

  /* Reply message */
  reply.args.create.inode_nr = inode->i_ino;
  reply.args.create.mode = inode->i_mode;
  reply.args.create.size = inode->i_size;
  reply.args.create.uid = inode->i_uid;
  reply.args.create.gid = inode->i_gid;
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
  //  sc = truncate_inode(struct inode *rip, off_t len)

  replymsg(portid, msgid, -EIO, NULL, 0);
}



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

 
/* @brief   Handle VFS CMD_CLOSE message
 *
 * @param   fsreq, message header received by getmsg.
 *
 * Release any resources of a previously opened/looked up file.
 * For extfs this shouldn't be applicable.
 */
void ext2_close(struct fsreq *req)
{
  replymsg(portid, msgid, 0, NULL, 0);
}


/* @brief   Handle VFS CMD_RENAME message
 *
 * @param   fsreq, message header received by getmsg.
 */
void ext2_rename(struct fsreq *req)
{
  char src_name[NAME_MAX+1];
  char dst_name[NAME_MAX+1];
  struct inode *src_dir_inode;
  struct inode *dst_dir_inode;
  size_t src_name_sz;
  size_t dst_name_sz;
  ino_t ino_nr;
  struct inode *inode;
  int sc;
    
  src_name_sz = req->args.rename.src_name_sz;
  dst_name_sz = req->args.rename.dst_name_sz;
  
  readmsg(portid, msgid, src_name, src_name_sz, sizeof *req);
  readmsg(portid, msgid, dst_name, dst_name_sz, sizeof *req + src_name_sz);

  src_dir_inode = get_inode(req->args.rename.src_dir_inode_nr);
  
  if (src_dir_inode == NULL) {
    replymsg(portid, msgid, -ENOENT, NULL, 0);
    return;
  }

  dst_dir_inode = get_inode(req->args.rename.dst_dir_inode_nr);
  
  if (dst_dir_inode == NULL) {
    put_inode(src_dir_inode);
    replymsg(portid, msgid, -ENOENT, NULL, 0);
    return;
  }

  sc = lookup_dir(src_dir_inode, src_name, &ino_nr);

  if (sc == 0) {
    inode = get_inode(ino_nr);
  }

  if (sc != 0 || inode == NULL) {
    put_inode(src_dir_inode);
    put_inode(dst_dir_inode);

    replymsg(portid, msgid, -EIO, NULL, 0);
    return;
  }

  sc = dirent_enter(dst_dir_inode, dst_name, ino_nr, inode->i_mode);

  if (sc == 0) {
	  inode->i_links_count++;
	  inode->i_update |= CTIME;
	  inode_markdirty(inode);
	  
    sc = dirent_delete(src_dir_inode, src_name);

    if (sc == 0) {
	    inode->i_links_count--;
	    inode->i_update |= CTIME;
      inode_markdirty(inode);
    }
  }
  
  put_inode(inode);
  put_inode(dst_dir_inode);
  put_inode(src_dir_inode);

  replymsg(portid, msgid, sc, NULL, 0);
}


/* @brief   Handle VFS CMD_MKNOD message
 *
 * @param   fsreq, message header received by getmsg.
 */
void ext2_mknod(struct fsreq *req)
{
  struct inode *dir_inode;  
  struct inode *inode;
  char name[NAME_MAX+1];
  int sc;
  mode_t mode;
  uid_t uid;
  gid_t gid;
  
  mode = req->args.mknod.mode;
  uid = req->args.mknod.uid;
  gid = req->args.mknod.gid;
   
  readmsg(portid, msgid, name, req->args.mknod.name_sz, sizeof *req);

  dir_inode = get_inode(req->args.mknod.dir_inode_nr);
  
  if (dir_inode == NULL) {
    replymsg(portid, msgid, -ENOENT, NULL, 0);
    return;
  }
  
  sc = new_inode(dir_inode, name, mode, uid, gid, &inode);

  if (sc != 0) {
    put_inode(dir_inode);
    replymsg(portid, msgid, sc, NULL, 0);
    return;
  }

  put_inode(inode);
  put_inode(dir_inode);

  replymsg(portid, msgid, 0, NULL, 0);
}


/* @brief   Handle VFS CMD_UNLINK message
 *
 * @param   fsreq, message header received by getmsg.

 * Unlink 'file_name'; rip must be the inode of 'file_name' or NULL.
 
 */
void ext2_unlink(struct fsreq *req)
{
  struct inode *dir_inode;
  struct inode *inode;
  char name[NAME_MAX+1];
  ino_t ino_nr;
  int	sc;
 
  readmsg(portid, msgid, name, req->args.unlink.name_sz, sizeof *req);

  dir_inode = get_inode(req->args.unlink.dir_inode_nr);
  
  if (dir_inode == NULL) {
    replymsg(portid, msgid, -EIO, NULL, 0);
    return;
  }
  
  sc = lookup_dir(dir_inode, name, &ino_nr);

  if (sc == 0) {
    inode = get_inode(ino_nr);
  }

  if (sc != 0 || inode == NULL) {
    put_inode(dir_inode);
    replymsg(portid, msgid, -EIO, NULL, 0);
    return;
  }
	  
  sc = dirent_delete(dir_inode, name);

  if (sc == 0) {
	  inode->i_links_count--;
	  inode->i_update |= CTIME;
	  inode_markdirty(inode);
  }

  put_inode(inode);
  put_inode(dir_inode);

  replymsg(portid, msgid, sc, NULL, 0);
}



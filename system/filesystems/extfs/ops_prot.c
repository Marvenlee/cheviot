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


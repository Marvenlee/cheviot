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


/* @brief   Check if a directory is empty
 *
 * @param   dir_inode, inode of directory to check if empty
 * @return  true if directory is empty, false otherwise
 */
bool is_dir_empty(struct inode *dir_inode)
{
  struct buf *bp = NULL;
  off_t pos = 0;
  int r = 0;
  
  while(pos < dir_inode->i_size) {
	  if(!(bp = get_dir_block(dir_inode, pos))) {
		  panic("extfs: is_dir_empty found a hole in a directory");
    }
    
    if (is_dir_block_empty(bp) == false) {
      put_block(cache, bp);
      return false;
    }
	  
	  put_block(cache, bp);
    pos += sb_block_size;
  }

  return true;
}


/* @brief   Search a directory block.
 *
 * @param   bp, pointer to a cached directory block
 * @return  true if block has no files, false otherwise
 */
bool is_dir_block_empty(struct buf *bp)
{
  struct dir_entry *dp;
    
  dp = (struct dir_entry*) bp->data;
  
  while(CUR_DISC_DIR_POS(dp, bp->data) < sb_block_size) {	  
    if (dp->d_ino != NO_ENTRY) {
	    if (strcmp_nz(dp->d_name, ".", dp->d_name_len) != 0 &&
	        strcmp_nz(dp->d_name, "..", dp->d_name_len) != 0) {
  	    /* not empty, dir contains something other than "." and ".." */
	      return false;
      }
    }

	  dp = NEXT_DISC_DIR_DESC(dp);
  }

  return true;
}


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


/*
 *
 */
uint32_t ext2_count_dirs(struct superblock *sp)
{
  uint32_t count = 0;
  unsigned int i;

  for (i = 0; i < sb_groups_count; i++) {
	  struct group_desc *desc = get_group_desc(i);

	  if (desc != NULL) {
  	  count += desc->g_used_dirs_count;
    }
  }

  return count;
}


/* @brief   Get a group descriptor
 *
 * @param   bnum, index into group descriptor table
 * @return  group descriptor
 */
struct group_desc *get_group_desc(unsigned int bnum)
{
  if (bnum >= sb_groups_count) {
	  log_error("extfs: get_group_desc: bnum invalid");
	  return NULL;
  }
  
  return &group_descs[bnum];
}


/* @brief   Copy multiple group descriptors
 *
 */
void copy_group_descriptors(struct group_desc *dest_array, 
                                   struct group_desc *source_array,
                                   unsigned int ngroups)
{
  for (unsigned int i = 0; i < ngroups; i++) {
  	gd_copy(&dest_array[i], &source_array[i]);
  }
}


/* @brief   Copy a group descriptor and byte-swap fields if needed
 *
 */
void gd_copy(struct group_desc *dest, struct group_desc *source)
{
  dest->g_block_bitmap      = bswap4(be_cpu, source->g_block_bitmap);
  dest->g_inode_bitmap      = bswap4(be_cpu, source->g_inode_bitmap);
  dest->g_inode_table       = bswap4(be_cpu, source->g_inode_table);
  dest->g_free_blocks_count = bswap2(be_cpu, source->g_free_blocks_count);
  dest->g_free_inodes_count = bswap2(be_cpu, source->g_free_inodes_count);
  dest->g_used_dirs_count   = bswap2(be_cpu, source->g_used_dirs_count);
}


void group_descriptors_markdirty(void)
{
  sb_group_descriptors_dirty = true;
}

void group_descriptors_markclean(void)
{
  sb_group_descriptors_dirty = false;
}


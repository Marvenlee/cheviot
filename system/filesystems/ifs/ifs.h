/*
 * Copyright 2014  Marven Gilhespie
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

#ifndef IFS_H
#define IFS_H

//#define NDEBUG

#include <sys/types.h>
#include <stdint.h>


// Constants
#define DIRENTS_BUF_SZ 4096

#define ALIGN_UP(val, alignment)                                \
    ((((val) + (alignment) - 1)/(alignment))*(alignment))
#define ALIGN_DOWN(val, alignment)                              \
            ((val) - ((val) % (alignment)))


/* @brief   IFS image superblock
 */
struct IFSHeader
{
  char magic[4];
  uint32_t node_table_offset;
  int32_t node_cnt;
  uint32_t ifs_size;
} __attribute__((packed));


/* @brief   IFS image inode
 */
struct IFSNode
{
  char name[32];
  int32_t inode_nr;
  int32_t parent_inode_nr;
  uint32_t permissions;
  int32_t uid;
  int32_t gid;
  uint32_t file_offset;
  uint32_t file_size;
} __attribute__((packed));


/* @brief   Information on physical address and size of IFS image
 *
 * This is passed to IFS process by the kernel
 */
struct Config
{
  void *ifs_image;
  size_t ifs_image_size;
};


// Prototypes
void init_ifs(int argc, char *argv[]);
void mount_root(void);


#endif


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

#include <sys/syscalls.h>
#include <sys/types.h>
#include "ext2.h"
#include "globals.h"

int block_fd;                       /* file descriptor of block-special device filesystem is on */
int portid;                         /* msgport created by mount() */
int kq;                             /* kqueue for receiving events */
msgid_t msgid;                      /* msgid of current message */

bool be_cpu;                        /* true if cpu is big-endian and we should byte-swap fields */

struct block_cache *cache;          /* file system block cache */
struct superblock superblock;
struct superblock ondisk_superblock;

struct group_desc *group_descs;
struct group_desc *ondisk_group_descs;

uint32_t  sb_inodes_per_block;     /* Number of inodes per block */
uint32_t  sb_inode_table_blocks_per_group;  /* Number of inode table blocks per group */
uint32_t  sb_group_desc_block_count;        /* Number of group descriptor blocks */
uint32_t  sb_desc_per_block;       /* Number of group descriptors per block */
uint32_t  sb_groups_count;         /* Number of groups in the filesystem */
uint8_t   sb_blocksize_bits;       /* Used to calculate offsets */

uint16_t  sb_block_size;           /* block size in bytes. */
uint16_t  sb_sectors_in_block;     /* sb_block_size / 512 */
uint32_t  sb_max_size;             /* maximum file size on this device */

uint32_t  sb_dirs_counter;
uint64_t  sb_gdt_position;

// Additional fields from command line settings
dev_t     sb_dev;                  /* superblock dev number */
bool      sb_rd_only;              /* set to true if filesystem is to be read-only */
int       sb_uid;
int       sb_gid;
int       sb_mode;
char      *sb_mount_path;
char      *sb_device_path;

// Computed values for searching indirect block tables
long      sb_addr_in_block;
long      sb_addr_in_block2;
long      sb_doub_ind_s;
long      sb_triple_ind_s;
long      sb_out_range_s;

uint32_t  sb_first_ino;
size_t    sb_inode_size;

bool      sb_group_descriptors_dirty;

const uint8_t zero_block_data[4096] = {0};

inode_list_t unused_inode_list;
inode_list_t hash_inodes[INODE_HASH_SIZE];
struct inode inode_cache[NR_INODES];


/* Ext2 global variables header.
 *
 * Created (CheviotOS Filesystem Handler based)
 *   December 2023 (Marven Gilhespie) 
 */

#ifndef EXT2FS_GLOBALS_H
#define EXT2FS_GLOBALS_H

#include <sys/syscalls.h>
#include <sys/types.h>
#include "ext2.h"

extern int block_fd;                /* file descriptor of block-special device filesystem is on */
extern int portid;                  /* msgport created by mount() */
extern int kq;                      /* kqueue for receiving events */
extern msgid_t msgid;               /* msgid of current message */

bool be_cpu;                        /* true if cpu is big-endian and we should byte-swap fields */

struct block_cache *cache;          /* file system block cache */
extern struct superblock superblock;
extern struct superblock ondisk_superblock;

extern struct group_desc *group_descs;
extern struct group_desc *ondisk_group_descs;

extern uint32_t  sb_inodes_per_block;     /* Number of inodes per block */
extern uint32_t  sb_inode_table_blocks_per_group; /* Number of inode table blocks per group */
extern uint32_t  sb_group_desc_block_count;       /* Number of group descriptor blocks */
extern uint32_t  sb_desc_per_block;       /* Number of group descriptors per block */
extern uint32_t  sb_groups_count;         /* Number of groups in the filesystem */
extern uint8_t   sb_blocksize_bits;       /* Used to calculate offsets */

extern uint16_t  sb_block_size;           /* block size in bytes. */
extern uint16_t  sb_sectors_in_block;     /* sb_block_size / 512 */
extern uint32_t  sb_max_size;             /* maximum file size on this device */

extern uint32_t  sb_dirs_counter;
extern uint64_t  sb_gdt_position;

// Additional fields from command line settings

extern dev_t     sb_dev;                  /* superblock dev number */
extern bool      sb_rd_only;              /* set to true if filesystem is to be read-only */

extern int       sb_uid;
extern int       sb_gid;
extern int       sb_mode;
extern char      *sb_mount_path;
extern char      *sb_device_path;

// Computed values for searching indirect block tables
extern long      sb_addr_in_block;
extern long      sb_addr_in_block2;
extern long      sb_doub_ind_s;
extern long      sb_triple_ind_s;
extern long      sb_out_range_s;

extern uint32_t  sb_first_ino;
extern size_t    sb_inode_size;

extern bool      sb_group_descriptors_dirty;

  
// Miscellaneous buffers
extern const uint8_t zero_block_data[4096];

// Lists
extern inode_list_t unused_inode_list;
extern inode_list_t hash_inodes[INODE_HASH_SIZE];
extern struct inode inode_cache[NR_INODES];


#endif

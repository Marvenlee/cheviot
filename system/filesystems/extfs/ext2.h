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

#ifndef EXT2FS_H
#define EXT2FS_H

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/debug.h>
#include <sys/fsreq.h>
#include <sys/lists.h>
#include <sys/mount.h>
#include <sys/panic.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/syscalls.h>
#include <sys/syslimits.h>
#include <sys/blockdev.h>
#include <unistd.h>

/*
 * Types
 */
typedef uint32_t bitchunk_t;
LIST_TYPE(inode, inode_list_t, inode_link_t);

/*
 * Config settings, tweak as needed
 */
#define NR_CACHE_BLOCKS          64         /* Keep 64 blocks in the local block cache */
#define NR_INODES                64         /* size of cached inode table */
#define INODE_HASH_SIZE         128


/*
 * Miscellaneous
 */ 
#define SUPER_MAGIC         0xEF53          /* magic number contained in super-block */

#define NO_BLOCK            ((block_t) 0)   /* absence of a block number */
#define NO_ENTRY            ((ino_t) 0)     /* absence of a dir entry */
#define NO_INODE            ((ino_t) 0)     /* no inode when allocating from bitmap */
#define NO_DEV              ((dev_t) 0)     /* absence of a device numb */
#define INVAL_UID           ((uid_t) -1)    /* invalid uid value */
#define INVAL_GID           ((gid_t) -1)    /* invalid gid value */
#define SU_UID              ((uid_t) 0)     /* super_user's uid_t */
#define NO_GROUP            ((uint32_t)-1)


/*
 * Superblock state (superblock.s_state)
 */
#define  EXT2_VALID_FS      0x0001    /* Unmounted cleanly */
#define  EXT2_ERROR_FS      0x0002    /* Errors detected */


/*
 * Revision levels
 */
#define EXT2_GOOD_OLD_REV   0         /* The good old (original) format */
#define EXT2_DYNAMIC_REV    1         /* V2 format w/ dynamic inode sizes */
#define EXT2_CURRENT_REV    EXT2_GOOD_OLD_REV
#define EXT2_MAX_SUPP_REV   EXT2_DYNAMIC_REV


/* 
 * Offsets and limits
 */

#define EXT2_NAME_MAX           255         /* Maximum filename length */
#define EXT2_LINK_MAX         32000         /* Maximum links to a file */

#define MAX_FILE_POS  ((off_t) 0x7FFFFFFF)  /* Maximum file offset */

#define SUPERBLOCK_OFFSET      1024         /* Offset in bytes of superblock from start of partition */
#define SUPERBLOCK_SIZE        1024         /* size of superblock stored on disk */

#define BOOT_BLOCK    ((block_t) 0)         /* block number of boot block */
#define START_BLOCK   ((block_t) 2)         /* first block of FS (not counting SB) */

#define BLOCK_ADDRESS_BYTES       4         /* bytes per address */

#define EXT2_MIN_BLOCK_SIZE         1024
#define EXT2_MAX_BLOCK_SIZE         4096
#define EXT2_MIN_BLOCK_LOG_SIZE     10


/* 
 * Inodes 
 */
#define EXT2_GOOD_OLD_INODE_SIZE    128
#define EXT2_GOOD_OLD_FIRST_INO     11


/*
 * Special inode numbers
 */
#define EXT2_BAD_INO                1     /* Bad blocks inode */
#define EXT2_ROOT_INO               2     /* Root inode */
#define EXT2_BOOT_LOADER_INO        5     /* Boot loader inode */
#define EXT2_UNDEL_DIR_INO          6     /* Undelete directory inode */

#define MAX_INODE_NR  ((ino_t)0xFFFFFFFF) /* largest inode number */


/* 
 * Inode block indicies and constants
 */
#define EXT2_NDIR_BLOCKS        12
#define EXT2_IND_BLOCK          EXT2_NDIR_BLOCKS
#define EXT2_DIND_BLOCK         (EXT2_IND_BLOCK + 1)
#define EXT2_TIND_BLOCK         (EXT2_DIND_BLOCK + 1)
#define EXT2_N_BLOCKS           (EXT2_TIND_BLOCK + 1)


/*
 * Inode i_mode field value to indicate inode is free
 */
#define INODE_FREE              0000000  


/*
 * Inode updating flags (inode.i_update)
 */
#define ATIME       0x0001    /* set to update atime when putting inode */
#define CTIME       0x0002    /* set to update ctime when putting inode */
#define MTIME       0x0004    /* set to update mtime when putting inode */


/*
 * Inode flags (inode.i_flags)
 */
#define EXT2_SECRM_FL             0x00000001 /* Secure deletion */
#define EXT2_UNRM_FL              0x00000002 /* Undelete */
#define EXT2_COMPR_FL             0x00000004 /* Compress file */
#define EXT2_SYNC_FL              0x00000008 /* Synchronous updates */
#define EXT2_IMMUTABLE_FL         0x00000010 /* Immutable file */
#define EXT2_APPEND_FL            0x00000020 /* writes to file may only append */
#define EXT2_NODUMP_FL            0x00000040 /* do not dump file */
#define EXT2_NOATIME_FL           0x00000080 /* do not update atime */
/* Reserved for compression usage... */
#define EXT2_DIRTY_FL             0x00000100
#define EXT2_COMPRBLK_FL          0x00000200 /* One or more compressed clusters */
#define EXT2_NOCOMP_FL            0x00000400 /* Don't compress */
/* End compression flags --- maybe not all used */
#define EXT2_ENCRYPT_FL           0x00000800 /* Encrypted file */
#define EXT2_BTREE_FL             0x00001000 /* btree format dir */
#define EXT2_INDEX_FL             0x00001000 /* hash-indexed directory */
#define EXT2_IMAGIC_FL            0x00002000 /* AFS directory */
#define EXT2_JOURNAL_DATA_FL      0x00004000 /* Reserved for ext3 */
#define EXT2_NOTAIL_FL            0x00008000 /* file tail should not be merged */
#define EXT2_DIRSYNC_FL           0x00010000 /* dirsync behaviour (directories only) */
#define EXT2_TOPDIR_FL            0x00020000 /* Top of directory hierarchies*/
#define EXT2_HUGE_FILE_FL         0x00040000 /* Reserved for ext4 */
#define EXT2_EXTENT_FL            0x00080000 /* Extents */
#define EXT2_VERITY_FL            0x00100000 /* Verity protected inode */
#define EXT2_EA_INODE_FL          0x00200000 /* Inode used for large EA */
#define EXT2_EOFBLOCKS_FL         0x00400000 /* Reserved for ext4 */
#define EXT2_NOCOW_FL             0x00800000 /* Do not cow file */
#define EXT2_DAX_FL               0x02000000 /* Inode is DAX */
#define EXT2_INLINE_DATA_FL       0x10000000 /* Reserved for ext4 */
#define EXT2_PROJINHERIT_FL       0x20000000 /* Create with parents projid */
#define EXT2_CASEFOLD_FL          0x40000000 /* Folder is case insensitive */
#define EXT2_RESERVED_FL          0x80000000 /* reserved for ext2 lib */

#define EXT2_FL_USER_VISIBLE      0x0003DFFF /* User visible flags */
#define EXT2_FL_USER_MODIFIABLE   0x000380FF /* User modifiable flags */


/*
 * Macros for handling dirents (borrowed from Minix)
 */
#define MIN_DIR_ENTRY_SIZE          8
#define DIR_ENTRY_ALIGN             4     
#define EXT2_DIR_PAD                4
#define EXT2_DIR_ROUND              (EXT2_DIR_PAD - 1)
#define EXT2_DIR_REC_LEN(name_len)  (((name_len) + 8 + EXT2_DIR_ROUND) & ~EXT2_DIR_ROUND)
#define EXT2_MAX_REC_LEN            ((1<<16)-1)

#define DIR_ENTRY_CONTENTS_SIZE(d) (MIN_DIR_ENTRY_SIZE + (d)->d_name_len)

/* size with padding */
#define DIR_ENTRY_ACTUAL_SIZE(d) (DIR_ENTRY_CONTENTS_SIZE(d) + \
                                  ((DIR_ENTRY_CONTENTS_SIZE(d) & 0x03) == 0 ? 0 : \
                                   DIR_ENTRY_ALIGN - (DIR_ENTRY_CONTENTS_SIZE(d) & 0x03) ))

/* Dentry can have padding, which can be used to enlarge namelen */
#define DIR_ENTRY_MAX_NAME_LEN(d)  (bswap2(be_cpu, (d)->d_rec_len) - MIN_DIR_ENTRY_SIZE)

/* Current position in block */
#define CUR_DISC_DIR_POS(cur_desc, base)  ((char*)cur_desc - (char*)base)

/* Return pointer to the next dentry */
#define NEXT_DISC_DIR_DESC(cur_desc)  ((struct dir_entry*) ((char*)cur_desc + cur_desc->d_rec_len))

/* Return next dentry's position in block */
#define NEXT_DISC_DIR_POS(cur_desc, base) (cur_desc->d_rec_len + CUR_DISC_DIR_POS(cur_desc, base))

/* Max size of a fast symlink embedded in inode, includes trailing '\0' */
#define MAX_FAST_SYMLINK_LENGTH (sizeof(((d_inode *)0)->i_block[0]) * EXT2_N_BLOCKS)


/*
 * Creator OS values (superblock.s_creator_os)
 */
#define EXT2_OS_LINUX       0
#define EXT2_OS_HURD        1
#define EXT2_OS_MASIX       2
#define EXT2_OS_FREEBSD     3
#define EXT2_OS_LITES       4


/*
 * Ext2 directory file types (dir_entry.d_file_type)
 */
#define EXT2_FT_UNKNOWN     0x00
#define EXT2_FT_REG_FILE    0x01
#define EXT2_FT_DIR         0x02
#define EXT2_FT_CHRDEV      0x03
#define EXT2_FT_BLKDEV      0x04
#define EXT2_FT_FIFO        0x05
#define EXT2_FT_SOCK        0x06
#define EXT2_FT_SYMLINK     0x07
#define EXT2_FT_MAX         0x08


/*
 * Feature set compatibility testing macros
 */
#define HAS_COMPAT_FEATURE(sb,mask)       (superblock.s_feature_compat & (mask))
#define HAS_RO_COMPAT_FEATURE(sb,mask)    (superblock.s_feature_ro_compat & (mask))
#define HAS_INCOMPAT_FEATURE(sb,mask)     (superblock.s_feature_incompat & (mask))

#define SET_COMPAT_FEATURE(sb,mask)       superblock.s_feature_compat |= (mask)
#define SET_RO_COMPAT_FEATURE(sb,mask)    superblock.s_feature_ro_compat |= (mask)
#define SET_INCOMPAT_FEATURE(sb,mask)     superblock.s_feature_incompat |= (mask)

#define CLEAR_COMPAT_FEATURE(sb,mask)     superblock.s_feature_compat &= ~(mask)
#define CLEAR_RO_COMPAT_FEATURE(sb,mask)  superblock.s_feature_ro_compat &= ~(mask)
#define CLEAR_INCOMPAT_FEATURE(sb,mask)   superblock.s_feature_incompat &= ~(mask)


/* 
 * Ext2 features
 */
#define EXT2_FEATURE_COMPAT_DIR_PREALLOC    0x0001
#define EXT2_FEATURE_COMPAT_IMAGIC_INODES   0x0002
#define EXT3_FEATURE_COMPAT_HAS_JOURNAL     0x0004
#define EXT2_FEATURE_COMPAT_EXT_ATTR        0x0008
#define EXT2_FEATURE_COMPAT_RESIZE_INO      0x0010
#define EXT2_FEATURE_COMPAT_DIR_INDEX       0x0020
#define EXT2_FEATURE_COMPAT_ANY             0xFFFFFFFF

#define EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER 0x0001
#define EXT2_FEATURE_RO_COMPAT_LARGE_FILE   0x0002
#define EXT2_FEATURE_RO_COMPAT_BTREE_DIR    0x0004
#define EXT2_FEATURE_RO_COMPAT_ANY          0xFFFFFFFF

#define EXT2_FEATURE_INCOMPAT_COMPRESSION   0x0001
#define EXT2_FEATURE_INCOMPAT_FILETYPE      0x0002
#define EXT3_FEATURE_INCOMPAT_RECOVER       0x0004
#define EXT3_FEATURE_INCOMPAT_JOURNAL_DEV   0x0008
#define EXT2_FEATURE_INCOMPAT_META_BG       0x0010
#define EXT2_FEATURE_INCOMPAT_ANY           0xFFFFFFFF


/*
 * Ext2 Features we support
 */
#define SUPPORTED_COMPAT_FEATURES       (0)

#define SUPPORTED_INCOMPAT_FEATURES     (EXT2_FEATURE_INCOMPAT_FILETYPE)

#define SUPPORTED_RO_COMPAT_FEATURES    (EXT2_FEATURE_RO_COMPAT_SPARSE_SUPER | \
                                         EXT2_FEATURE_RO_COMPAT_LARGE_FILE)

#define UNSUPPORTED_INCOMPAT_FEATURES   ~SUPPORTED_INCOMPAT_FEATURES
#define UNSUPPORTED_RO_COMPAT_FEATURES  ~SUPPORTED_RO_COMPAT_FEATURES


/*
 * Structure of a blocks group descriptor
 */
struct group_desc
{
  uint32_t  g_block_bitmap;           /* 0, Blocks bitmap block */
  uint32_t  g_inode_bitmap;           /* 4, Inodes bitmap block */
  uint32_t  g_inode_table;            /* 8, Inodes table block */
  uint16_t  g_free_blocks_count;      /* 12, Free blocks count */
  uint16_t  g_free_inodes_count;      /* 14, Free inodes count */
  uint16_t  g_used_dirs_count;        /* 16, Directories count */
  uint16_t  pad;
  uint32_t  reserved[3];              /* 20, */
} __attribute__ ((packed));           /* Total size, 32 bytes */


/*
 * Structure of an inode
 */
struct inode
{
  uint16_t  i_mode;               /* 0, File mode */
  uint16_t  i_uid;                /* 2, Low 16 bits of Owner Uid */
  uint32_t  i_size;               /* 4, Size in bytes */
  uint32_t  i_atime;              /* 8, Access time */
  uint32_t  i_ctime;              /* 12, Creation time */
  uint32_t  i_mtime;              /* 16, Modification time */
  uint32_t  i_dtime;              /* 20, Deletion Time */
  uint16_t  i_gid;                /* 24, Low 16 bits of Group Id */
  uint16_t  i_links_count;        /* 26, Links count */
  uint32_t  i_blocks;             /* 28, Blocks count (512-byte blocks) */
  uint32_t  i_flags;              /* 32, File flags */
  union {
    struct {
      uint32_t  l_i_reserved1;    /* 36, */
    } linux1;
    struct {
      uint32_t  h_i_translator;   /* 36, */
    } hurd1;
    struct {
      uint32_t  m_i_reserved1;    /* 36, */
    } masix1;
  } osd1;                         /* 36, osd1 */
  
  uint32_t  i_block[EXT2_N_BLOCKS]; /* 40, Pointers to blocks (15 * uin32t_t) */
  uint32_t  i_generation;         /* 100, File version (for NFS) */
  uint32_t  i_file_acl;           /* 104, File ACL */
  uint32_t  i_dir_acl;            /* 108, Directory ACL */
  uint32_t  i_faddr;              /* 112, Fragment address */
  union {
    struct {
      uint8_t   l_i_frag;         /* 116, Fragment number */
      uint8_t   l_i_fsize;        /* 117, Fragment size */
      uint16_t  i_pad1;           
      uint16_t  l_i_uid_high;     /* 120, these 2 fields    */
      uint16_t  l_i_gid_high;     /* 122, were reserved2[0] */
      uint32_t  l_i_reserved2;    /* 124 */
    } linux2;
    struct {
      uint8_t   h_i_frag;         /* 116, Fragment number */
      uint8_t   h_i_fsize;        /* 117, Fragment size */
      uint16_t  h_i_mode_high;    /* 118, */
      uint16_t  h_i_uid_high;     /* 120, */
      uint16_t  h_i_gid_high;     /* 122, */
      uint32_t  h_i_author;       /* 124, */
    } hurd2;
    struct {
      uint8_t   m_i_frag;         /* 116, Fragment number */
      uint8_t   m_i_fsize;        /* 117, Fragment size */
      uint16_t  m_pad1;           
      uint32_t  m_i_reserved2[2]; /* 120, */
    } masix2;
  } osd2;                         /* 116, osd2 */

  /* Total size, 128 bytes for above on disk inode fields */  

  /* The following metadata items are not present on disk */

  inode_link_t    i_hash_link;    /* hash list */
  inode_link_t    i_unused_link;  /* free and unused list */

  ino_t   i_ino;                  /* inode number */

  int     i_count;                /* Reference count of in-memory inode */

  int     i_update;               /* ATIME, CTIME and MTIME to update when writing inode to disk */
  int     i_dirty;                /* inode is dirty */

} __attribute__ ((packed));


/*
 * Structure of the super block
 * 
 * This is 1024 bytes in size
 */
struct superblock
{
  uint32_t  s_inodes_count;           /*  0, Inodes count */
  uint32_t  s_blocks_count;           /*  4, Blocks count */
  uint32_t  s_r_blocks_count;         /*  8, Reserved blocks count */
  uint32_t  s_free_blocks_count;      /* 12, Free blocks count */
  uint32_t  s_free_inodes_count;      /* 16, Free inodes count */
  uint32_t  s_first_data_block;       /* 20, First Data Block */
  uint32_t  s_log_block_size;         /* 24, Block size */
  uint32_t  s_log_frag_size;          /* 28, Fragment size */
  uint32_t  s_blocks_per_group;       /* 32, # Blocks per group */
  uint32_t  s_frags_per_group;        /* 36, # Fragments per group */
  uint32_t  s_inodes_per_group;       /* 40, # Inodes per group */
  uint32_t  s_mtime;                  /* 44, Mount time */
  uint32_t  s_wtime;                  /* 48, Write time */
  uint16_t  s_mnt_count;              /* 52, Mount count */
  uint16_t  s_max_mnt_count;          /* 54, Maximal mount count */
  uint16_t  s_magic;                  /* 56, Magic signature */
  uint16_t  s_state;                  /* 58, File system state */
  uint16_t  s_errors;                 /* 60, Behaviour when detecting errors */
  uint16_t  s_minor_rev_level;        /* 62, Minor revision level */
  uint32_t  s_lastcheck;              /* 64, Time of last check */
  uint32_t  s_checkinterval;          /* 68, Max. time between checks */
  uint32_t  s_creator_os;             /* 72, Creator OS */
  uint32_t  s_rev_level;              /* 76, Revision level */
  uint16_t  s_def_resuid;             /* 80, Default uid for reserved blocks */
  uint16_t  s_def_resgid;             /* 82, Default gid for reserved blocks */
  
  /*
   * These fields are for EXT2_DYNAMIC_REV superblocks only.
   *
   * Note: the difference between the compatible feature set and
   * the incompatible feature set is that if there is a bit set
   * in the incompatible feature set that the kernel doesn't
   * know about, it should refuse to mount the filesystem.
   * 
   * e2fsck's requirements are more strict; if it doesn't know
   * about a feature in either the compatible or incompatible
   * feature set, it must abort and not try to meddle with
   * things it doesn't understand...
   */
  uint32_t  s_first_ino;              /* 84, First non-reserved inode */
  uint16_t  s_inode_size;             /* 88, size of inode structure */
  uint16_t  s_block_group_nr;         /* 90, block group # of this superblock */
  uint32_t  s_feature_compat;         /* 92, compatible feature set */
  uint32_t  s_feature_incompat;       /* 96, incompatible feature set */
  uint32_t  s_feature_ro_compat;      /* 100, readonly-compatible feature set */
  uint8_t   s_uuid[16];               /* 104, 128-bit uuid for volume */
  char      s_volume_name[16];        /* 120, volume name */
  char      s_last_mounted[64];       /* 136, directory where last mounted */
  uint32_t  s_algorithm_usage_bitmap; /* 200, For compression */

  /*
   * Performance hints.  Directory preallocation should only
   * happen if the EXT2_COMPAT_PREALLOC flag is on.
   */
  uint8_t   s_prealloc_blocks;        /* 204, Nr of blocks to try to preallocate*/
  uint8_t   s_prealloc_dir_blocks;    /* 205, Nr to preallocate for dirs */
  uint16_t  s_padding1;

  /*
   * Journaling support valid if EXT3_FEATURE_COMPAT_HAS_JOURNAL set.
   */
  uint8_t   s_journal_uuid[16];       /* 208, uuid of journal superblock */
  uint32_t  s_journal_inum;           /* 224, inode number of journal file */
  uint32_t  s_journal_dev;            /* 228, device number of journal file */
  uint32_t  s_last_orphan;            /* 232, start of list of inodes to delete */
  uint32_t  s_hash_seed[4];           /* 236, HTREE hash seed */
  uint8_t   s_def_hash_version;       /* 252, Default hash version to use */
  uint8_t   s_reserved_char_pad;
  uint16_t  s_reserved_word_pad;
  uint32_t  s_default_mount_opts;     /* 256, */
  uint32_t  s_first_meta_bg;          /* 260, First metablock block group */
  uint32_t  s_reserved[190];          /* 264, Padding to the end of the block */
} __attribute__ ((packed));           /* Total size, 1024 bytes */



/*
 * The new version of the directory entry.  Since EXT2 structures are
 * stored in intel byte order, and the name_len field could never be
 * bigger than 255 chars, it's safe to reclaim the extra byte for the
 * file_type field.
 */
struct dir_entry
{
  uint32_t  d_ino;
  uint16_t  d_rec_len;
  uint8_t   d_name_len;
  uint8_t   d_file_type;
  char      d_name[1];
} __attribute__ ((packed));


/*
 * Structure to manage the filling of the readdir buffer
 */
struct dirent_buf
{
  char *data;
  size_t size;
  int position;
};


/*
 * Prototypes
 */

// bitmap.c
uint32_t alloc_bit(uint32_t *bitmap, uint32_t max_bits, uint32_t start_word);
int clear_bit(uint32_t *bitmap, uint32_t index);

// block.c
struct buf *new_block(struct inode *inode, off_t position);
block_t read_map_entry(struct inode *inode, uint64_t position);
int enter_map_entry(struct inode *inode, off_t position, block_t new_block);
int delete_map_entry(struct inode *inode, off_t position);
int calc_block_indirection_offsets(uint32_t position, uint32_t offs[4]);
int get_indirect_blocks(struct inode *inode, int depth, uint32_t offs[4], uint32_t block[4]);
uint32_t get_toplevel_indirect_block_entry(struct inode *inode, int depth);
void set_toplevel_indirect_block_entry(struct inode *inode, int depth, uint32_t block);
block_t read_indirect_block_entry(struct buf *bp, int index);
void write_indirect_block_entry(struct buf *bp, int wrindex, block_t block);
bool is_empty_indirect_block(struct buf *bp);
void zero_block(struct buf *bp);
block_t alloc_block(struct inode *inode, block_t block);
void free_block(block_t block);
void check_block_number(struct group_desc *gd, block_t block);

// dir.c
ssize_t get_dirents(struct inode *dino_nr, off64_t *cookie, char *buf, ssize_t sz);
struct buf *get_dir_block(struct inode *inode, off64_t position);
struct dir_entry *seek_to_valid_dirent(struct buf *bp, off_t temp_pos, off_t pos);
bool fill_dirent_buf(struct buf *bp, struct dir_entry **d_desc, struct dirent_buf *db);
unsigned int get_dtype(struct dir_entry *dp);
void dirent_buf_init(struct dirent_buf *db, void *data, size_t sz);
int dirent_buf_add(struct dirent_buf *db, int ino_nr, char *name, int len);
size_t dirent_buf_finish(struct dirent_buf *db);
int strcmp_nz(char *s1_nz, char *s2, size_t s1_len);

// dir_delete.c
int dirent_delete(struct inode *dir_inode, char *name);
int search_block_and_delete(struct inode *dir_inode, struct buf *bp, char *name);
void delete_dir_entry(struct inode *dir_inode, struct dir_entry *dp, 
                      struct dir_entry *prev_dp, struct buf *bp);

// dir_enter.c
int dirent_enter(struct inode *dir_inode, char *name, ino_t ino_nr, mode_t mode);
int find_dirent_free_space(struct inode *dir_inode, struct buf *bp,
                             size_t required_space, struct dir_entry **ret_dp);                                         
int enter_dirent(struct inode *dir_inode, struct buf *bp, struct dir_entry *dp,
                 ino_t ino_nr, char *name, size_t name_len, mode_t mode);                
struct dir_entry *extend_directory(struct inode *dir_inode, struct buf **bpp);
struct dir_entry *shrink_dir_entry(struct dir_entry *dp, struct buf *bp);
void set_dirent_file_type(struct dir_entry *dp, mode_t mode);

// dir_isempty.c
bool is_dir_empty(struct inode *dir_inode);
bool is_dir_block_empty(struct buf *bp);

// dir_lookup.c
int lookup_dir(struct inode *dir_inode, char *name, ino_t *numb); 
int lookup_dir_block(struct inode *dir_inode, struct buf *bp,
                     char *name, ino_t *ret_ino_nr);

// group_descriptors.c
uint32_t ext2_count_dirs(struct superblock *sp);
struct group_desc *get_group_desc(unsigned int bnum);
void copy_group_descriptors(struct group_desc *dest_array,
                            struct group_desc *source_array,
                            unsigned int ngroups);
void gd_copy(struct group_desc *dest, struct group_desc *source);
void group_descriptors_markdirty(void);
void group_descriptors_markclean(void);

// init.c
void init(int argc, char *argv[]);
int process_args(int argc, char *argv[]);
int detect_ext2fs_partition(void);

// inode.c
int new_inode(struct inode *dir_inode, char *name, mode_t mode, 
                        uid_t uid, gid_t gid, struct inode **res);
int alloc_inode(struct inode *parent, mode_t bits, uid_t uid, gid_t gid, struct inode **res);
void free_inode(struct inode *inode);
uint32_t alloc_inode_bit(uint32_t group, bool is_dir);
void free_inode_bit(uint32_t ino_nr, bool is_dir);
uint32_t find_free_inode_dir_group(uint32_t parent_ino);
uint32_t find_free_inode_file_group(uint32_t parent_ino);


// inode_cache.c
int init_inode_cache(void);
void addhash_inode(struct inode *node);
void unhash_inode(struct inode *node);
struct inode *get_inode(ino_t numb);
struct inode *find_inode(ino_t numb);
void put_inode(struct inode *inode);
void update_times(struct inode *inode);
void read_inode(struct inode *inode);
void write_inode(struct inode *inode);
void inode_copy(struct inode *dst, struct inode *src);
void inode_markdirty(struct inode *inode);
void inode_markclean(struct inode *inode);


// main.c
int main(int argc, char *argv[]);

// ops_dir.c
void ext2_lookup(struct fsreq *req);
void ext2_readdir(struct fsreq *req);
void ext2_mkdir(struct fsreq *req);
void ext2_rmdir(struct fsreq *req);

// ops_file.c
void ext2_read(struct fsreq *req);
void ext2_write(struct fsreq *req);
void ext2_create(struct fsreq *req);
void ext2_truncate(struct fsreq *req);

// ops_link.c
void ext2_close(struct fsreq *req);
void ext2_rename(struct fsreq *req);
void ext2_mknod(struct fsreq *req);
void ext2_unlink(struct fsreq *req);

// ops_prot.c
void ext2_chmod(struct fsreq *req);
void ext2_chown(struct fsreq *req);

// read.c
ssize_t read_file(ino_t ino_nr, size_t nrbytes, off64_t position);
int read_chunk(struct inode *inode, off64_t position, size_t off, size_t chunk, size_t msg_off);
int read_nonexistent_block(size_t msg_off, size_t len);

// superblock.c
int read_superblock(void);
void write_superblock(void);
void super_copy(struct superblock *dest, struct superblock *source);

// truncate.c
int truncate_inode(struct inode *inode, ssize_t sz);

// utility.c
void determine_cpu_endianness(void);
uint16_t bswap2(bool norm, uint16_t w);
uint32_t bswap4(bool norm, uint32_t x);

// write.c
ssize_t write_file(ino_t ino_nr, size_t nrbytes, off64_t position);
int write_chunk(struct inode *inode, off64_t position, size_t off, size_t chunk, size_t msg_off);

#endif

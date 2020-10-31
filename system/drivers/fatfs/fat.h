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

#ifndef FAT_H
#define FAT_H

#include <sys/syslimits.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fsreq.h>
#include <sys/lists.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/syscalls.h>
#include <unistd.h>
/*
 * Constants
 */

#define FAT_TIME_CREATE 0
#define FAT_TIME_MODIFY 1
#define FAT_TIME_ACCESS 2

#define TYPE_FAT12 0
#define TYPE_FAT16 1
#define TYPE_FAT32 2

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_FILENAME                                                     \
  (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

#define CLUSTER_FREE 0x00000000
#define CLUSTER_ALLOC_MIN 0x00000001
#define CLUSTER_ALLOC_MAX 0x0ffffff6
#define CLUSTER_BAD 0x0ffffff7
#define CLUSTER_EOC 0x0fffffff

#define FAT12_CLUSTER_FREE 0x00000000
#define FAT12_CLUSTER_ALLOC_MIN 0x00000001
#define FAT12_CLUSTER_ALLOC_MAX 0x00000ff6
#define FAT12_CLUSTER_BAD 0x00000ff7
#define FAT12_CLUSTER_EOC_MIN 0x00000ff8
#define FAT12_CLUSTER_EOC_MAX 0x00000fff
#define FAT12_CLUSTER_EOC 0x00000fff

#define FAT16_CLUSTER_FREE 0x00000000
#define FAT16_CLUSTER_ALLOC_MIN 0x00000001
#define FAT16_CLUSTER_ALLOC_MAX 0x0000fff6
#define FAT16_CLUSTER_BAD 0x0000fff7
#define FAT16_CLUSTER_EOC_MIN 0x0000fff8
#define FAT16_CLUSTER_EOC_MAX 0x0000ffff
#define FAT16_CLUSTER_EOC 0x0000ffff

#define FAT32_CLUSTER_FREE 0x00000000
#define FAT32_CLUSTER_ALLOC_MIN 0x00000001
#define FAT32_CLUSTER_ALLOC_MAX 0x0ffffff6
#define FAT32_CLUSTER_BAD 0x0ffffff7
#define FAT32_CLUSTER_EOC_MIN 0x0ffffff8
#define FAT32_CLUSTER_EOC_MAX 0x0fffffff
#define FAT32_CLUSTER_EOC 0x0fffffff

#define DIRENTRY_FREE 0x00
#define DIRENTRY_DELETED 0xe5
#define DIRENTRY_LONG 0xe5

#define FAT32_RESVD_SECTORS 32
#define FAT16_ROOT_DIR_ENTRIES 512
#define FAT32_BOOT_SECTOR_BACKUP_SECTOR_START 6
#define FAT32_BOOT_SECTOR_BACKUP_SECTOR_CNT 3
#define BPB_EXT_OFFSET 36
#define FAT16_BOOTCODE_START 0x3e
#define FAT32_BOOTCODE_START 0x5a
#define SIZEOF_FAT32_BOOTCODE 134
#define SIZEOF_FAT16_BOOTCODE 134

#define FSINFO_LEAD_SIG 0x41615252
#define FSINFO_STRUC_SIG 0x61417272
#define FSINFO_TRAIL_SIG 0xaa550000

#define FAT_DIRENTRY_SZ 32

/*
 *
 */

struct MBRPartitionEntry {
  uint8_t status;
  uint8_t chs[3];
  uint8_t partition_type;
  uint8_t chs_last[3];
  uint32_t lba;
  uint32_t nsectors;
} __attribute__((packed));

/*
 * Structures
 */

struct FatBPB {
  uint8_t jump[3];
  char oem_name[8];
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t reserved_sectors_cnt;
  uint8_t fat_cnt;
  uint16_t root_entries_cnt;
  uint16_t total_sectors_cnt16;
  uint8_t media_type;
  uint16_t sectors_per_fat16;
  uint16_t sectors_per_track;
  uint16_t heads_per_cylinder;
  uint32_t hidden_sectors_cnt;
  uint32_t total_sectors_cnt32;

} __attribute__((__packed__));

/*
 *
 */

struct FatBPB_16Ext {
  uint8_t drv_num;
  uint8_t reserved1;
  uint8_t boot_sig;
  uint32_t volume_id;
  uint8_t volume_label[11];
  uint8_t filesystem_type[8];

} __attribute__((__packed__));

/*
 *
 */

struct FatBPB_32Ext {
  uint32_t sectors_per_fat32;
  uint16_t ext_flags;
  uint16_t fs_version;
  uint32_t root_cluster;
  uint16_t fs_info;
  uint16_t boot_sector_backup;
  uint32_t reserved[12];
  uint8_t drv_num;
  uint8_t reserved1;
  uint8_t boot_sig;
  uint32_t volume_id;
  uint8_t volume_label[11];
  uint8_t filesystem_type[8];

} __attribute__((__packed__));

struct FatFSInfo {
  uint32_t lead_sig;
  uint32_t reserved1[120];
  uint32_t struc_sig;
  uint32_t free_cnt;
  uint32_t next_free;
  uint32_t reserved2[3];
  uint32_t trail_sig;

} __attribute__((__packed__));

/*
 * 32 bytes in size?
 */

struct FatDirEntry {
  unsigned char name[8];
  unsigned char extension[3];
  uint8_t attributes;
  uint8_t reserved;
  uint8_t creation_time_sec_tenths;
  uint16_t creation_time_2secs;
  uint16_t creation_date;
  uint16_t last_access_date;
  uint16_t first_cluster_hi;

  uint16_t last_write_time;
  uint16_t last_write_date;

  uint16_t first_cluster_lo;
  uint32_t size;
} __attribute__((__packed__));

/*
 *
 */

struct FatNode {
  ino_t inode_nr;
  bool is_root;
  uint32_t dirent_sector;
  uint32_t dirent_offset;
  struct FatDirEntry dirent;

  uint32_t hint_cluster; /* Seek hint, hint_cluster = 0 for invalid hint */
  uint32_t hint_offset;

  int reference_cnt;
  LIST_ENTRY(FatNode) node_entry;
  struct FatSB *fsb;
};

/*
 *
 */

struct FatSB {
  int fat_type;

  struct FatBPB bpb;
  struct FatBPB_16Ext bpb16;
  struct FatBPB_32Ext bpb32;
  struct FatFSInfo fsi;

  struct FatNode root_node;
  //	int diskchange_signal;
  //	int flush_signal;

  //	struct Device *device;
  //	void *unitp;

  uint32_t features;

  /* Where is it initialized? */
  uint32_t total_sectors;

  //	struct Buf *buf;

  int partition_start; /* Partition start/end */
  int partition_size;
  uint32_t total_sectors_cnt;

  uint32_t sectors_per_fat; /* ????????????? */
  uint32_t data_sectors;    /* ???????????? Used or not ? */
  uint32_t cluster_cnt;     /* ???????????? Used or not ? */

  uint32_t first_data_sector;     /* Computed at BPB validation */
  uint32_t root_dir_sectors;      /* Computed at BPB validation */
  uint32_t first_root_dir_sector; /* Computed at BPB validation */
  uint32_t start_search_cluster;

  uint32_t search_start_cluster; /* ????????? free fat entries? */
  uint32_t last_cluster; /* ????????? last fat entry,  not is FatBPB ?? */

  LIST(FatNode)
  node_list;
};

/*
 * Fat Formatting tables
 */

struct Fat12BPBSpec {
  uint16_t bytes_per_sector;     /* sector size */
  uint8_t sectors_per_cluster;   /* sectors per cluster */
  uint16_t reserved_sectors_cnt; /* reserved sectors */
  uint8_t fat_cnt;               /* FATs */
  uint16_t root_entries_cnt;     /* root directory entries */
  uint16_t total_sectors_cnt16;  /* total sectors */
  uint8_t media_type;            /* media descriptor */
  uint16_t sectors_per_fat16;    /* sectors per FAT */
  uint16_t sectors_per_track;    /* sectors per track */
  uint16_t heads_per_cylinder;   /* drive heads */
};

/*
 *
 */

struct FatDskSzToSecPerClus {
  uint32_t disk_size;
  uint32_t sectors_per_cluster;
};

#define DIRENTS_BUF_SZ 4096

struct Config {
  char mount_path[PATH_MAX + 1];
  char device_path[PATH_MAX + 1];
  uid_t uid;
  gid_t gid;
  mode_t mode;
  bool fat_format;
};




#define BUF_HASH_CNT			64

struct Blk;


struct Cache
{
  int block_fd;
  
	uint32_t buffer_cnt;
	uint32_t block_size;
	uint32_t lba_start;
	uint32_t lba_end;
	int writethru_critical;
	uint32_t writeback_delay;
	uint32_t max_transfer;
		
	struct Blk *blk_table;
	void *blk_mem;
	
	LIST (Blk) lru_list;
	LIST (Blk) dirty_list;
	LIST (Blk) free_list;
	LIST (Blk) hash_list[BUF_HASH_CNT];
};




/*
 * struct Blk
 */

struct Blk
{
	off64_t sector;
	uint32_t dirty;
	struct Cache *cache;
	void *mem;
	bool in_use;
	
	LIST_ENTRY (Blk) lru_entry;
	LIST_ENTRY (Blk) dirty_entry;
	LIST_ENTRY (Blk) free_entry;
	LIST_ENTRY (Blk) hash_entry;
};




/*
 * Write mode flags
 */

#define BUF_IMMED				(1<<0)
#define BUF_ONESHOT				(1<<1)




/*
 * VM Macros
 */

#define ALIGN_UP(val, alignment)                                               \
  ((((val) + (alignment)-1) / (alignment)) * (alignment))

#define ALIGN_DOWN(val, alignment) ((val) - ((val) % (alignment)))


/*
 * Variables
 */
 
extern struct Fat12BPBSpec fat12_bpb[];
extern struct FatDskSzToSecPerClus dsksz_to_spc_fat16[];
extern struct FatDskSzToSecPerClus dsksz_to_spc_fat32[];


/*
 * Prototypes
 */
 
struct FatNode *FindNode(ino_t inode_nr);
void InitRootNode();
struct FatNode *AllocNode(struct FatDirEntry *dirent, uint32_t sector,
                          uint32_t offset);
void FreeNode(struct FatNode *node);
int FlushDirent(struct FatNode *node);
int FlushFSB(void);
void FlushFSInfo(void);

int fat_search_dir(char *component);

int fat_dir_read(struct FatNode *node, void *buf, off_t offset,
                 uint32_t *r_sector, uint32_t *r_sector_offset);
int fat_cmp_dirent(struct FatDirEntry *dirent, char *comp);
int fat_read_fat(uint32_t cluster, uint32_t *r_value);
int fat_find_cluster(struct FatNode *node, off_t offset, uint32_t *r_cluster);

uint32_t fat_get_first_cluster(struct FatDirEntry *dirent);
char *fat_path_advance(char *next, char *component);




// main.c
void fatLookup(int pid, struct fsreq *fsreq);
void fatClose(int pid, struct fsreq *fsreq);
void fatCreate(int ino, struct fsreq *req);
void fatRead(int pid, struct fsreq *fsreq);
void fatWrite(int pid, struct fsreq *req);
void fatReadDir(int pid, struct fsreq *req);
void fatUnlink(int pid, struct fsreq *req);
void fatRmDir(int pid, struct fsreq *req);
void fatMkDir(int ino, struct fsreq *fsreq);
void fatMkNod(int pid, struct fsreq *req);
void fatRename(int pid, struct fsreq *req);


// cluster.c
int ReadFATEntry(uint32_t cluster, uint32_t *r_value);
int WriteFATEntry(uint32_t cluster, uint32_t value);
int AppendCluster(struct FatNode *node, uint32_t *r_cluster);
int FindLastCluster(struct FatNode *node, uint32_t *r_cluster);
int FindCluster(struct FatNode *node, off_t offset, uint32_t *r_cluster);
uint32_t GetFirstCluster(struct FatDirEntry *dirent);
void SetFirstCluster(struct FatDirEntry *dirent, uint32_t cluster);
int FindFreeCluster(uint32_t *r_cluster);
void FreeClusters(uint32_t first_cluster);
uint32_t ClusterToSector(uint32_t cluster);
void FileOffsetToSectorOffset(struct FatNode *node, off_t file_offset,
                              uint32_t *r_sector, uint32_t *r_sec_offset);
int ClearCluster(uint32_t cluster);

// dir.c
int FatCreateDirEntry(struct FatNode *parent, struct FatDirEntry *dirent,
                      uint32_t *r_sector, uint32_t *r_sector_offset);
void FatDeleteDirEntry(uint32_t sector, uint32_t sector_offset);

int FatDirRead(struct FatNode *node, void *buf, off_t offset, uint32 *r_sector,
               uint32 *r_sector_offset);

// file.c
size_t readFile(struct FatNode *node, void *buf, size_t count, off_t offset);
size_t writeFile(struct FatNode *node, void *buf, size_t count, off_t offset);
struct FatNode *createFile(struct FatNode *parent, char *name);
int truncateFile(struct FatNode *node, size_t size);
int extendFile(struct FatNode *node, size_t length);

// format.c
int FatFormat (char *label, uint32_t flags, uint32_t cluster_size);
int InitializeFatSB (struct FatDirEntry *label_dirent, uint32_t flags, uint32_t cluster_size);
int FatEraseDisk (uint32_t flags);
int FatWriteBootRecord (void);
int FatInitFATs (void);
int FatInitRootDirectory (struct FatDirEntry *label_dirent);
void FatPrecalculateFSBValues (void);

// init.c
void init(int argc, char *argv[]);
int processArgs(int argc, char *argv[]);
int detectPartition(void);

// lookup.c
int lookup (struct FatNode *dirnode, char *name, struct FatNode **r_node);


// sector.c
int readSector(void *buf, int sector, int sector_offset, size_t nbytes);
int writeSector(void *buf, int sector, int sector_offset, size_t nbytes);
int blockRead(void *buf, size_t sz, int block_no);
int blockWrite(void *buff, size_t sz, int block_no);
struct Cache *CreateCache (int block_fd, uint32_t buffer_cnt, uint32_t block_size,
						uint32_t lba_start, uint32_t lba_end, int writethru_critical, uint32_t writeback_delay,
						uint32_t max_transfer);
void FreeCache (struct Cache *cache);
void SyncCache (struct Cache *cache);
void InvalidateCache (struct Cache *cache);
int BufReadBlocks (struct Cache *cache, void *addr, uint32_t block, uint32_t offset, uint32_t nbytes);
int BufWriteBlocks (struct Cache *cache, void *addr, uint32_t block, uint32_t offset, uint32_t nbytes, int mode);
struct Blk *BufGetBlock (struct Cache *cache, uint32_t block);
struct Blk *BufPutBlock (struct Cache *cache, struct Blk *blk, int mode);







int FlushDirent(struct FatNode *node);

int FatDirEntryToASCIIZ(char *pathbuf, struct FatDirEntry *dirent);


int FatASCIIZToDirEntry(struct FatDirEntry *dirent, char *filename);
int FatIsDosName(char *s);
int IsDirEmpty(struct FatNode *node);
















#endif

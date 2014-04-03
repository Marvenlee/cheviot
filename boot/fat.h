#ifndef FAT_H
#define FAT_H

#include "types.h"
#include "lists.h"


/*
 * Constants
 */

#define FAT_TIME_CREATE					0
#define FAT_TIME_MODIFY					1
#define FAT_TIME_ACCESS					2

#define TYPE_FAT12						0
#define TYPE_FAT16						1
#define TYPE_FAT32						2

#define ATTR_READ_ONLY					0x01
#define ATTR_HIDDEN						0x02
#define ATTR_SYSTEM						0x04
#define ATTR_VOLUME_ID					0x08
#define ATTR_DIRECTORY					0x10
#define ATTR_ARCHIVE					0x20
#define ATTR_LONG_FILENAME				(ATTR_READ_ONLY | ATTR_HIDDEN |	ATTR_SYSTEM | ATTR_VOLUME_ID)

#define CLUSTER_FREE					0x00000000
#define CLUSTER_ALLOC_MIN				0x00000001
#define CLUSTER_ALLOC_MAX				0x0ffffff6
#define CLUSTER_BAD						0x0ffffff7
#define CLUSTER_EOC						0x0fffffff

#define FAT12_CLUSTER_FREE				0x00000000
#define FAT12_CLUSTER_ALLOC_MIN			0x00000001
#define FAT12_CLUSTER_ALLOC_MAX			0x00000ff6
#define FAT12_CLUSTER_BAD				0x00000ff7
#define FAT12_CLUSTER_EOC_MIN			0x00000ff8
#define FAT12_CLUSTER_EOC_MAX			0x00000fff
#define FAT12_CLUSTER_EOC				0x00000fff

#define FAT16_CLUSTER_FREE				0x00000000
#define FAT16_CLUSTER_ALLOC_MIN			0x00000001
#define FAT16_CLUSTER_ALLOC_MAX			0x0000fff6
#define FAT16_CLUSTER_BAD				0x0000fff7
#define FAT16_CLUSTER_EOC_MIN			0x0000fff8
#define FAT16_CLUSTER_EOC_MAX			0x0000ffff
#define FAT16_CLUSTER_EOC				0x0000ffff

#define FAT32_CLUSTER_FREE				0x00000000
#define FAT32_CLUSTER_ALLOC_MIN			0x00000001
#define FAT32_CLUSTER_ALLOC_MAX			0x0ffffff6
#define FAT32_CLUSTER_BAD				0x0ffffff7
#define FAT32_CLUSTER_EOC_MIN			0x0ffffff8
#define FAT32_CLUSTER_EOC_MAX			0x0fffffff
#define FAT32_CLUSTER_EOC				0x0fffffff

#define DIRENTRY_FREE					0x00
#define DIRENTRY_DELETED				0xe5
#define DIRENTRY_LONG					0xe5

#define FAT32_RESVD_SECTORS						32
#define FAT16_ROOT_DIR_ENTRIES  				512
#define FAT32_BOOT_SECTOR_BACKUP_SECTOR_START	6
#define FAT32_BOOT_SECTOR_BACKUP_SECTOR_CNT		3
#define BPB_EXT_OFFSET							36
#define FAT16_BOOTCODE_START 					0x3e
#define FAT32_BOOTCODE_START 					0x5a
#define SIZEOF_FAT32_BOOTCODE   				134
#define SIZEOF_FAT16_BOOTCODE   				134

#define FSINFO_LEAD_SIG			0x41615252
#define FSINFO_STRUC_SIG		0x61417272
#define FSINFO_TRAIL_SIG		0xaa550000

#define FAT_DIRENTRY_SZ							32



/*
 *
 */

struct MBRPartitionEntry
{
	uint8 status;
	uint8 chs[3];
	uint8 partition_type;
	uint8 chs_last[3];
	uint32 lba;
	uint32 nsectors;
} __attribute__ ((packed));





/*
 * Structures
 */
 
struct FatBPB 
{
	uint8	jump[3];
	char	oem_name[8];
	uint16	bytes_per_sector;
	uint8	sectors_per_cluster;
	uint16	reserved_sectors_cnt;
	uint8	fat_cnt;
	uint16	root_entries_cnt;
	uint16	total_sectors_cnt16;
	uint8	media_type;
	uint16	sectors_per_fat16;
	uint16	sectors_per_track;
	uint16	heads_per_cylinder;
	uint32	hidden_sectors_cnt;
	uint32	total_sectors_cnt32;
  
} __attribute__ (( __packed__ ));




/*
 *
 */

struct FatBPB_16Ext
{
	uint8	drv_num;
	uint8	reserved1;
	uint8	boot_sig;
	uint32	volume_id;
	uint8	volume_label[11];
	uint8	filesystem_type[8];

} __attribute__ (( __packed__ ));




/*
 *
 */

struct FatBPB_32Ext
{
  	uint32	sectors_per_fat32;	
  	uint16	ext_flags;	
  	uint16	fs_version;
  	uint32	root_cluster;
  	uint16	fs_info;
  	uint16	boot_sector_backup;
  	uint32	reserved[12];
  	uint8	drv_num;
  	uint8	reserved1;
	uint8	boot_sig;
	uint32	volume_id;
	uint8	volume_label[11];
	uint8	filesystem_type[8];
	
} __attribute__ (( __packed__ ));
			
			
			
		

struct FatFSInfo
{
	uint32 lead_sig;
	uint32 reserved1[120];
	uint32 struc_sig;
	uint32 free_cnt;
	uint32 next_free;
	uint32 reserved2[3];
	uint32 trail_sig;

} __attribute__ (( __packed__ ));




/*
 *
 */

struct FatDirEntry
{
  unsigned char		name[8];
  unsigned char		extension[3];
  uint8		attributes;
  uint8		reserved;
  uint8		creation_time_sec_tenths;
  uint16	creation_time_2secs;
  uint16	creation_date;
  uint16	last_access_date;
  uint16	first_cluster_hi;

  uint16	last_write_time;
  uint16	last_write_date;

  uint16	first_cluster_lo;
  uint32	size;
}  __attribute__ (( __packed__ ));




/*
 *
 */

struct FatNode
{
	bool is_root;
	uint32 dirent_sector;
	uint32 dirent_offset;
	struct FatDirEntry dirent;
	
	uint32 hint_cluster;			/* Seek hint, hint_cluster = 0 for invalid hint */
	uint32 hint_offset;


};




/*
 *
 */

struct FatSB
{
	int fat_type;

	struct FatBPB bpb;
	struct FatBPB_16Ext bpb16;
	struct FatBPB_32Ext bpb32;
	struct FatFSInfo fsi;
	
//	int diskchange_signal;
//	int flush_signal;
	
//	struct Device *device;
//	void *unitp;
	
		
	uint32 features;
	
	/* Where is it initialized? */
	uint32 total_sectors;
	
//	struct Buf *buf;
	
	int partition_start;			/* Partition start/end */
	int partition_size;
	uint32 total_sectors_cnt;
	
	uint32 sectors_per_fat;			/* ????????????? */
	uint32 data_sectors;			/* ???????????? Used or not ? */
	uint32 cluster_cnt;				/* ???????????? Used or not ? */
	
	uint32 first_data_sector;		/* Computed at BPB validation */
	uint32 root_dir_sectors;		/* Computed at BPB validation */
	uint32 first_root_dir_sector;	/* Computed at BPB validation */
	uint32 start_search_cluster;
	
	
	uint32 search_start_cluster; 	 /* ????????? free fat entries? */
	uint32 last_cluster;         	 /* ????????? last fat entry,  not is FatBPB ?? */
};





/*
 *
 */

struct FatFilp
{
	uint32 offset;
};




/*
 * Fat Formatting tables
 */

struct Fat12BPBSpec
{ 
   uint16 bytes_per_sector;			/* sector size */ 
   uint8 sectors_per_cluster;		/* sectors per cluster */ 
   uint16 reserved_sectors_cnt;		/* reserved sectors */ 
   uint8 fat_cnt;					/* FATs */ 
   uint16 root_entries_cnt;			/* root directory entries */ 
   uint16 total_sectors_cnt16;		/* total sectors */ 
   uint8 media_type;				/* media descriptor */ 
   uint16 sectors_per_fat16;		/* sectors per FAT */ 
   uint16 sectors_per_track;		/* sectors per track */ 
   uint16 heads_per_cylinder;		/* drive heads */ 
};




/*
 *
 */

struct FatDskSzToSecPerClus
{
	uint32 disk_size;
	uint32 sectors_per_cluster;
};








/*
 * Global variables
 */

extern struct Fat12BPBSpec fat12_bpb[];
extern struct FatDskSzToSecPerClus dsksz_to_spc_fat16[];
extern struct FatDskSzToSecPerClus dsksz_to_spc_fat32[];


extern char fat_no_name_label[11];
extern char fat12_filesystem_type_str[8];
extern char fat16_filesystem_type_str[8];
extern char fat32_filesystem_type_str[8];		

extern uint8 fat32_bootcode[];
extern uint8 fat16_bootcode[];






int fat_init (void);
int fat_open (char *pathname);
void fat_close (void);
void fat_seek (off_t offset);
size_t fat_read (void *buf, size_t count);
int fat_lookup (char *pathname);
int fat_search_dir (char *component);
int fat_dir_read (void *buf, off_t offset, uint32 *r_sector, uint32 *r_sector_offset);
int fat_cmp_dirent (struct FatDirEntry *dirent, char *comp);
int fat_read_fat (uint32 cluster, uint32 *r_value);
int fat_find_cluster (off_t offset, uint32 *r_cluster);
uint32 fat_get_first_cluster (struct FatDirEntry *dirent);
char *fat_path_advance (char *next, char *component);
int fat_read_sector (void *buf, int sector, int sector_offset, size_t nbytes);

size_t fat_getline (char *buf, size_t buf_sz);
int fat_getch(void);
size_t fat_get_filesize (void);



#endif


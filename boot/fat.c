#include "fat.h"
#include <string.h>
#include "globals.h"
#include "memory.h"
#include "dbg.h"







/*
 *
 */

int fat_init (void)
{
	struct MBRPartitionEntry mbe[4];
	int t;

	sd_read(bdev, bootsector, 512, 0);
	
	KLOG ("bootsector end = %#02x %#02x", bootsector[510], bootsector[511]);
	KLOG ("****************************************************");
	
	bmemcpy (mbe, bootsector + 446, 16 * 4);


	for (t=0; t<4; t++)
	{
		KLOG ("partition = %d", t);
		
		if (mbe[t].partition_type == 0x00)
			continue;

		fsb.partition_start = mbe[t].lba;
		fsb.partition_size = mbe[t].nsectors;
		
		KLOG ("Partition %d, sec = %d, sz = %d",  t, mbe[t].lba, mbe[t].nsectors);

		sd_read(bdev, file_buf, 512, fsb.partition_start);
		file_buf_sector = 0;
				
		bmemcpy (&fsb.bpb, file_buf, sizeof (struct FatBPB));
		bmemcpy (&fsb.bpb16, file_buf + BPB_EXT_OFFSET, sizeof (struct FatBPB_16Ext));
		bmemcpy (&fsb.bpb32, file_buf + BPB_EXT_OFFSET, sizeof (struct FatBPB_32Ext));
		
		KLOG ("bytes_per_sector = %d", fsb.bpb.bytes_per_sector);
		
		if (fsb.bpb.bytes_per_sector != 512)
			continue;
		
		KLOG ("sectors_per_cluster = %d", fsb.bpb.sectors_per_cluster);
		
		if (!	(fsb.bpb.sectors_per_cluster >= 1
				&& fsb.bpb.sectors_per_cluster <= 128
				&& (fsb.bpb.sectors_per_cluster & (fsb.bpb.sectors_per_cluster - 1)) == 0))
			continue;
		
		
		KLOG ("reserved_sectors_cnt = %d", fsb.bpb.reserved_sectors_cnt);
		
		if (fsb.bpb.reserved_sectors_cnt == 0)
			continue;

		KLOG ("fat_cnt = %d", fsb.bpb.fat_cnt);
				
		if (fsb.bpb.fat_cnt == 0)
			continue;
		
		KLOG ("media_type = %d", fsb.bpb.media_type);
		
		/*
		if (!	(fsb.bpb.media_type == 0
				|| fsb.bpb.media_type == 1
				|| fsb.bpb.media_type >= 0xf0))
			continue;
		*/	

/*		
		if (!	( (fsb.bpb.total_sectors_cnt16 != 0)
				|| (fsb.bpb.total_sectors_cnt16 == 0 && fsb.bpb.total_sectors_cnt32 != 0)))
			continue;
		
		if (!	((fsb.bpb.sectors_per_fat16 != 0)
				|| (fsb.bpb.sectors_per_fat16 == 0 && fsb.bpb32.sectors_per_fat32 != 0)))
			continue;
*/

		fsb.root_dir_sectors = ((fsb.bpb.root_entries_cnt * sizeof (struct FatDirEntry))
									+ (512 - 1)) / 512;
		
		if (fsb.bpb.sectors_per_fat16 != 0)
			fsb.sectors_per_fat = fsb.bpb.sectors_per_fat16;
		else
			fsb.sectors_per_fat = fsb.bpb32.sectors_per_fat32;
		
		if (fsb.bpb.total_sectors_cnt16 != 0)
			fsb.total_sectors_cnt = fsb.bpb.total_sectors_cnt16;
		else
			fsb.total_sectors_cnt = fsb.bpb.total_sectors_cnt32;
		
		
		fsb.first_data_sector = fsb.bpb.reserved_sectors_cnt
					+ (fsb.bpb.fat_cnt * fsb.sectors_per_fat)
					+ fsb.root_dir_sectors;
		
		fsb.data_sectors = fsb.total_sectors_cnt - (fsb.bpb.reserved_sectors_cnt
									+ (fsb.bpb.fat_cnt * fsb.sectors_per_fat)
									+ fsb.root_dir_sectors);
		
		fsb.cluster_cnt = fsb.data_sectors / fsb.bpb.sectors_per_cluster;
		
		if (fsb.cluster_cnt < 4085)
		{
			KLOG ("FAT12");
			fsb.fat_type = TYPE_FAT12;
			continue;
		}
		else if (fsb.cluster_cnt < 65525)
		{
			fsb.fat_type = TYPE_FAT16;
			KLOG ("FAT16");
		}
		else
		{
			fsb.fat_type = TYPE_FAT32;
			KLOG ("FAT32");
		}
					
		if (fsb.fat_type == TYPE_FAT32 && fsb.bpb32.fs_version != 0)
			continue;
		
		if ((fsb.fat_type == TYPE_FAT12 || fsb.fat_type == TYPE_FAT16) && fsb.bpb.root_entries_cnt == 0)
			continue;

		KLOG ("root_entries_cnt = %d", fsb.bpb.root_entries_cnt);	

		KLOG ("FAT PARTITION FOUND");	
		return 0;
		
	}
	
	KLOG ("No FAT partition");
	while (1);
}








/*
 * FatOpen();
 */

int fat_open (char *pathname)
{
	KLOG ("fat_open (%s)", pathname);
	
	if (fat_lookup (pathname) != 0)
	{
		KLOG ("fat_open FAILED");
		return -1;
	}

	if (node.dirent.attributes & ATTR_DIRECTORY)
	{
		KLOG ("fat_open FAILED IS_DIR");
		return -1;
	}

	KLOG ("fat_open SUCCESS");
	filp.offset = 0;
	return 0;
}





/*
 * FatSeek();
 */

void fat_seek (off_t offset)
{
	filp.offset = offset;
}





/*
 *
 */

size_t fat_read (void *buf, size_t count)
{
	uint32 cluster;
	uint32 nbytes_read;
	uint32 transfer_nbytes;
	uint32 sector;
	uint32 cluster_offset;
	uint32 sector_offset;
	uint8 *dst;
	
	if ((node.dirent.attributes & ATTR_DIRECTORY) != 0)
		return -1;
	
	if (filp.offset >= node.dirent.size)
		count = 0;
	else if (count < (node.dirent.size - filp.offset))
		count = count;
	else
		count = node.dirent.size - filp.offset;
	
	dst = buf;
	nbytes_read = 0;
	
	while (nbytes_read < count)
	{	
		if (fat_find_cluster (filp.offset, &cluster) != 0)
			break;
		
		cluster_offset = filp.offset % (fsb.bpb.sectors_per_cluster * 512);
		
		sector = ((cluster - 2) * fsb.bpb.sectors_per_cluster) + fsb.first_data_sector;
		sector += (cluster_offset / 512);
		
		
		sector_offset = filp.offset % 512;
		
		transfer_nbytes = (512 < (count - nbytes_read)) ? 512 : (count - nbytes_read);
		transfer_nbytes = (transfer_nbytes < (512 - sector_offset))
						? transfer_nbytes : (512 - sector_offset);
		
		
		if (fat_read_sector (dst, sector, sector_offset, transfer_nbytes) != 0)
			break;
		
		
		dst += transfer_nbytes;
		nbytes_read += transfer_nbytes;
		filp.offset += transfer_nbytes;
	}

	return nbytes_read;
}










/*
 * Would be nice to make it generic across filesystems, then supply pointers to
 * name comparison/conversion functions.
 */

int fat_lookup (char *pathname)
{
	char component[256];
	char *next;
	
	KLOG ("fat_lookup (%s)", pathname);

	node.is_root = TRUE;	
	
	next = pathname;
	next = fat_path_advance (next, component);
	KLOG ("component = %s, next = %s", component, next);

	while (*component != '\0')
	{	
		if (fat_search_dir (component) != 0)
			return -1;

		if (node.is_root == TRUE)
			node.is_root = FALSE;

		if (*next != '\0' && (node.dirent.attributes & ATTR_DIRECTORY) == 0)
			KPanic ("Not last component and not a DIRECTORY");

		next = fat_path_advance (next, component);
	
		KLOG ("next component = %s, next = %s", component, next);
	}
	
	return 0;
}






/*
 * FatSearchDir();
 */

int fat_search_dir (char *component)
{
	struct FatDirEntry dirent;
	int32 offset;
	uint32 dirent_sector, dirent_offset;
	int32 rc;
	
	offset = 0;
	
	KLOG ("fat_search_dir (%s)", component);
	
	do
	{
		rc = fat_dir_read (&dirent, offset, &dirent_sector, &dirent_offset);
		
		if (rc == 0)
		{
			if (dirent.name[0] == DIRENTRY_FREE)
				return -1;
			
			if (dirent.name[0] != DIRENTRY_DELETED)	
			{
				if (fat_cmp_dirent (&dirent, component) == 0)
				{
					node.is_root = 0;
					node.hint_cluster = 0;
					node.hint_offset = 0;
					memcpy (&node.dirent, &dirent, sizeof (dirent));
					return 0;
				}
			}
						
			offset ++;
		}
		
	} while (rc == 0);
	
	return -1;
}






/*
 *
 */

int fat_dir_read (void *buf, off_t offset, uint32 *r_sector, uint32 *r_sector_offset)
{
	uint32 cluster;
	uint32 sector;
	uint32 cluster_offset;
	uint32 sector_offset;

	
	
	if (node.is_root == TRUE && (fsb.fat_type == TYPE_FAT12 || fsb.fat_type == TYPE_FAT16))
	{
		if (offset < fsb.bpb.root_entries_cnt)
		{
			sector = fsb.bpb.reserved_sectors_cnt +
				(fsb.bpb.fat_cnt * fsb.sectors_per_fat) + ((offset * FAT_DIRENTRY_SZ)/512);
			sector_offset = (offset * FAT_DIRENTRY_SZ) % 512;
			
			if (fat_read_sector (buf, sector, sector_offset, FAT_DIRENTRY_SZ) == 0)
			{
				if (r_sector != NULL)
					*r_sector = sector;
				if (r_sector_offset != NULL)
					*r_sector_offset = sector_offset;

				return 0;
			}
		}
	}
	else
	{
		if (fat_find_cluster (offset * FAT_DIRENTRY_SZ, &cluster) == 0)
		{
			cluster_offset = (offset * FAT_DIRENTRY_SZ) % (fsb.bpb.sectors_per_cluster * 512);

			sector = ((cluster - 2) * fsb.bpb.sectors_per_cluster) + fsb.first_data_sector;
			sector += (cluster_offset / 512);
			sector_offset = (offset * FAT_DIRENTRY_SZ) % 512;
		
			if (fat_read_sector (buf, sector, sector_offset, FAT_DIRENTRY_SZ) == 0)
			{
				if (r_sector != NULL)
					*r_sector = sector;
				if (r_sector_offset != NULL)
					*r_sector_offset = sector_offset;
				
				return 0;
			}
		}
	}
	
	return -1;
}





/*
 * CompareDirEntry()
 *
 * Comparison can be simplified, shouldn't need len. (old?)
 */
 
int fat_cmp_dirent (struct FatDirEntry *dirent, char *comp)
{
	int i;
	char *comp_p, *dirent_p;
	char packed_dirent_name[13];
	char *p;	


	if (!(dirent->name[0] != DIRENTRY_FREE && (uint8)dirent->name[0] != DIRENTRY_DELETED
			&& (dirent->attributes & ATTR_LONG_FILENAME) != ATTR_LONG_FILENAME))
	{
		return -1;
	}
				
	p = packed_dirent_name;
	*p = '\0';

	for (i=0; i<8 && dirent->name[i] != ' '; i++)
		*p++ = dirent->name[i];
		
	if (dirent->extension[0] != ' ')
		*p++ = '.';
	
	for (i=0; i<3 && dirent->extension[i] != ' '; i++)
		*p++ = dirent->extension[i];

	*p++ = '\0';
	
	comp_p = comp;
	dirent_p = packed_dirent_name;
	
//	KLOG("compare : packed = %s, comp = %s", dirent_p, comp_p);
	
	for ( ; *comp_p == *dirent_p; comp_p++, dirent_p++)
		if (*comp_p == '\0')
			return 0;
	
//	KLOG ("NO MATCH");
    return -1;
}







/*
 *
 */

int fat_find_cluster (off_t offset, uint32 *r_cluster)
{
	uint32 cluster_size;
	uint32 temp_offset;
	uint32 cluster;
	
	cluster_size = fsb.bpb.sectors_per_cluster * 512;
	offset = (offset / cluster_size) * cluster_size;

	temp_offset = 0;
	
	if (node.hint_cluster != 0 && offset == node.hint_offset)
	{
		*r_cluster = node.hint_cluster;
		return 0;
	}
	
	if (node.hint_cluster != 0 && offset > node.hint_offset)
	{
		temp_offset = node.hint_offset;
		cluster = node.hint_cluster;
	}
	else if (node.is_root == TRUE)
		cluster = fsb.bpb32.root_cluster;
	else
		cluster = fat_get_first_cluster (&node.dirent);
	
	
	
	while (temp_offset < offset)
	{
		if (cluster < CLUSTER_ALLOC_MIN || cluster > CLUSTER_ALLOC_MAX)
		{	
			return -1;
		}

		if (fat_read_fat (cluster, &cluster) != 0)
			return -1;
		
		temp_offset += cluster_size;
	}
	
	if (cluster < CLUSTER_ALLOC_MIN || cluster > CLUSTER_ALLOC_MAX)
	{
		return -1;
	}
	else
	{
		*r_cluster = cluster;
		return 0;
	}
}







/*
 * ReadFATEntry();
 *
 * Modify to use 32-bit values for args and result.
 *
 * Things that call ReadEntry WriteEntry need better error checking.
 *
 * Use multiple FATs
 */

int fat_read_fat (uint32 cluster, uint32 *r_value)
{
	int32 sector, sector_offset;
	uint32 fat_offset;
	uint16 word_value;
	uint32 long_value;
	int f;
	uint32 fat_sz;
	
		
	for (f=0; f < fsb.bpb.fat_cnt; f++)
	{
		if (fsb.fat_type == TYPE_FAT16)
		{
			fat_sz = fsb.bpb.sectors_per_fat16;
			fat_offset = cluster * 2;
						
			sector = fsb.bpb.reserved_sectors_cnt + (fat_offset / 512) + (f * fat_sz);
			sector_offset = fat_offset % 512;
			
			if (fat_buf_sector != sector)
			{
				sd_read (bdev, fat_buf, 512, sector + fsb.partition_start);
				fat_buf_sector = sector;
			}
			bmemcpy (&word_value, fat_buf + sector_offset, 2);

			if (word_value >= FAT16_CLUSTER_EOC_MIN)
				*r_value = CLUSTER_EOC;
			else if (word_value == FAT16_CLUSTER_BAD)
				*r_value = CLUSTER_BAD;
			else
				*r_value = word_value;
			
			return 0;
		}
		else
		{
			fat_sz = fsb.bpb32.sectors_per_fat32;
			fat_offset = cluster * 4;
			
			sector = fsb.bpb.reserved_sectors_cnt + (fat_offset / 512)  + (f * fat_sz);
			sector_offset = fat_offset % 512;
			
			if (fat_buf_sector != sector)
			{
				sd_read (bdev, fat_buf, 512, sector + fsb.partition_start);
				fat_buf_sector = sector;
			}

			bmemcpy (&long_value, fat_buf + sector_offset, 4);

			if (long_value >= FAT32_CLUSTER_EOC_MIN && long_value <= FAT32_CLUSTER_EOC_MAX)
				*r_value = CLUSTER_EOC;
			else if (long_value == FAT32_CLUSTER_BAD)
				*r_value = CLUSTER_BAD;
			else
				*r_value = long_value;
			
			return 0;
		}
	}
	
	KLOG ("fat_read_fat() FAIL");
	return -1;
}













/*
 * GetFirstCluster();
 */

uint32 fat_get_first_cluster (struct FatDirEntry *dirent)
{
	uint32 cluster;

	if (fsb.fat_type == TYPE_FAT16)
	{
		cluster = dirent->first_cluster_lo;

		if (cluster >= FAT16_CLUSTER_EOC_MIN && cluster <= FAT16_CLUSTER_EOC_MAX)
			return CLUSTER_EOC;
		else if(cluster == FAT16_CLUSTER_BAD)
			return CLUSTER_BAD;
		else
			return cluster;
	}
	else
	{
		cluster = (dirent->first_cluster_hi<<16) + dirent->first_cluster_lo;
		
		if (cluster >= FAT32_CLUSTER_EOC_MIN && cluster <= FAT32_CLUSTER_EOC_MAX)
			return CLUSTER_EOC;
		else if(cluster == FAT32_CLUSTER_BAD)
			return CLUSTER_BAD;
		else
			return cluster;
	}
}











/*
 * Advance();
 *
 * Can be used within filesystems to parse a pathname.
 */

char *fat_path_advance (char *next, char *component)
{
	while (*next == '/')
		next++;

	*component = '\0';
		
	if (*next == '\0')
	{
		KLOG ("Next EOL");
		return next;
	}
	
	while (*next != '/' && *next != '\0')
		*component++ = *next++;
	
	*component = '\0';
	
	return next;
}






/*
 *
 */

int fat_read_sector (void *mem, int sector, int sector_offset, size_t nbytes)
{
	if (file_buf_sector != sector)
	{
		sd_read(bdev, file_buf, 512, fsb.partition_start + sector);
		file_buf_sector = sector;
	}
	
	bmemcpy (mem, file_buf + sector_offset, nbytes);
	return 0;
}








/*
 *
 */

size_t fat_getline (char *buf, size_t buf_sz)
{
	size_t nbytes_read = 0;
	int ch;
	

	while (1)
	{
		ch = fat_getch();
	
		*buf++ = (char)ch;

		if (ch == -1)
		{
			*buf = '\0';
			break;
		}
		
		nbytes_read ++;
				
		if (ch == '\n' || ch == '\r' || ch == '\0')
		{
			*buf = '\0';
			break;
		}
	}
	
	
	return nbytes_read;
}




int fat_getch(void)
{
	char ch;
	
	if (fat_read (&ch, 1) != 1)
		return -1;
	
	return ch;
}

















size_t fat_get_filesize (void)
{
	return node.dirent.size;
}
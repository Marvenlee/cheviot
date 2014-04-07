#ifndef INTERFACES_BLOCK_H
#define INTERFACES_BLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/syscalls.h>
#include <interfaces/port.h>




/*
 *
 */
 
#define BLK_CMD_STRATEGY            1
#define BLK_CMD_DISKINFO			2


#define BLK_DIR_READ				1
#define BLK_DIR_WRITE				2



/* Error codes
 */
 
#define IOERR_NOTSPECIFIED  -1
#define IOERR_OPENFAIL		-2  /* device/unit failed to open */
#define IOERR_ABORTED		-3  /* request aborted */
#define IOERR_NOCMD			-4  /* command not supported */
#define IOERR_BOUND         -5  /* Invalid sector/range of sectors */

#define IOERR_MEDIA_CHANGE  -6	/* CMD_MEDIA_PRESENT but others can return it as well */
#define IOERR_NO_MEDIA      -7  /* CMD_MEDIA_PRESENT but others can return it as well */
#define IOERR_WRITE_PROTECT -8  /* Write failed */
#define IOERR_BADMEDIA      -9  /* Write failed */
#define IOERR_TIMEOUT       -10
#define IOERR_SEEK          -11


/*
 * CD-ROM error codes
 */

#define CDERR_NOSENSE			0
#define CDERR_RECOVERED			1
#define CDERR_NOT_READY			2
#define CDERR_MEDIUM			3
#define CDERR_HARDWARE			4
#define CDERR_ILLEGAL			5
#define CDERR_ATTENTION			6
#define CDERR_PROTECT			7
#define CDERR_ABORT				11
#define CDERR_MISCOMPARE		14



/* GenDisk.flags
 */

#define GD_PARTITIONABLE		(1<<0)
#define GD_REMOVABLE			(1<<1)
#define GD_WRITEABLE			(1<<2)
#define GD_BOOTDRIVE			(1<<3)


#define DISKNAME_SZ				16
#define SERIALNAME_SZ			21
#define MODELNAME_SZ			41

struct GenDisk
{
	int major;			// DEV_MAJ_ATA
	int minor;			// unit * NPARTITION;
	char name[DISKNAME_SZ];
	char type[DISKNAME_SZ];
	char model[MODELNAME_SZ];
	char serial[SERIALNAME_SZ];
	uuid_t uuid;
	bits32_t flags;
	bool disk_changed;
	bool validated;
	uint64 capacity;			// Capacity in bytes
	uint32 sector_size;
};





struct MsgBlock
{
	int cmd;
	int error;
	
	union
	{
		struct
		{
			uint64 start_sector;
			uint32 nsectors;
			int direction;
			size_t buf_offset;
		} strategy;
	
		struct
		{
			struct GenDisk gendisk;
		} diskinfo;
	} s;
	
	
	union
	{
		int strategy;
		int diskinfo;
	} r;
	
};









#ifdef __cplusplus
}
#endif

#endif

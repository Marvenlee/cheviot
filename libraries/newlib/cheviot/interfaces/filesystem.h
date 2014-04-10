#ifndef INTERFACES_FILESYSTEM_H
#define INTERFACES_FILESYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/syscalls.h>
#include <unistd.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <interfaces/block.h>




/*
 * Constants
 */

#define MAX_PATHNAME_SZ		1024
#define FILESYSTEMNAME_SZ	16
#define FS_BUFFER_SZ		4096

#define FS_TIMEOUT			10

#define NR_FD				64


/*
 * Filesystem commands
 */

#define EXEC_CMD_OPEN					1
#define EXEC_CMD_CLOSE				    2
#define EXEC_CMD_READ					3
#define EXEC_CMD_WRITE					4
#define EXEC_CMD_LSEEK					5
#define EXEC_CMD_CHDIR					6
#define EXEC_CMD_FCHDIR					7
#define EXEC_CMD_MKDIR					8
#define EXEC_CMD_RMDIR					9
#define EXEC_CMD_OPENDIR				10
#define EXEC_CMD_READDIR				11
#define EXEC_CMD_REWINDDIR				12
#define EXEC_CMD_SYMLINK				13
#define EXEC_CMD_CHMOD					14
#define EXEC_CMD_CHOWN					15
#define EXEC_CMD_REMOVE					16
#define EXEC_CMD_RENAME					17
#define EXEC_CMD_FTRUNCATE				18
#define EXEC_CMD_TRUNCATE				19
#define EXEC_CMD_FSTAT					20
#define EXEC_CMD_STAT					21
#define EXEC_CMD_STATFS					22
#define EXEC_CMD_SYNC					23
#define EXEC_CMD_ACCESS					24

#define EXEC_CMD_RELABEL				25
#define EXEC_CMD_FORMAT					26
#define EXEC_CMD_GETVOLUMEGUID			27
#define EXEC_CMD_SETVOLUMEGUID			28
#define EXEC_CMD_READDEVICEENTRY		29
#define EXEC_CMD_READPARTITIONTABLE		30
#define EXEC_CMD_WRITEPARTITIONTABLE	31
#define EXEC_CMD_INSTALLMBRBOOTLOADER	32

#define EXEC_CMD_SETREMOVABLEATTRS		33
#define EXEC_CMD_SETPROGRAMDIR			34
#define EXEC_CMD_STARTNOTIFY			35
#define EXEC_CMD_ENDNOTIFY				36
#define EXEC_CMD_WAITNOTIFY				37
#define EXEC_CMD_DUP					38

#define EXEC_CMD_SETUID                 39
#define EXEC_CMD_GETUID                 40
#define EXEC_CMD_SETGID                 41
#define EXEC_CMD_GETGID                 42

#define EXEC_CMD_CREATEPORT             43
#define EXEC_CMD_LISTEN                 44
#define EXEC_CMD_CONNECT                45



/*
 *
 */
 
#define NPARTITION		128

struct Partition
{
	int64 start_sector;			// Logcal Block Address (sector) of partition
	int64 size;					// Number of sectors in partition.
	bool bootable;				// TRUE if device is bootable
	bool valid;					// TRUE if partition is valid
};


/*
 * struct DeviceEntry;
 */

struct DeviceEntry
{
	char *devicename [DISKNAME_SZ];
	struct GenDisk gendisk;	
};





/*
 * struct MsgFile;
 */

struct FileMsg
{
	int cmd;
	int error;
	
	union
	{
		struct
		{
			int whence;
			off_t offset;
		} lseek;
		
		
		struct
		{
			size_t core_sz;
		}  installmbrbootloader;
		
		struct
		{
			uid_t uid;
			gid_t gid;
		} setremovableattrs;
		
		struct
		{
			char pathname[MAX_PATHNAME_SZ];
			mode_t mode;
			int oflags;
		} open;
	
		struct
		{
			off_t offset;
			size_t nbyte;
		} read;
		
		struct
		{
			off_t offset;
			size_t nbyte;
		} write;
	
		struct
		{
			char pathname[MAX_PATHNAME_SZ];
		} chdir;
	
		struct
		{
			char pathname[MAX_PATHNAME_SZ];
			mode_t mode;
		} mkdir;
		
		struct
		{
			char pathname[MAX_PATHNAME_SZ];
		} rmdir;
	
		struct
		{
			char pathname[MAX_PATHNAME_SZ];
		} opendir;
		
		
		struct
		{
			char pathname[MAX_PATHNAME_SZ];
			char link[MAX_PATHNAME_SZ];
		} symlink;
	
		struct
		{
			char pathname[MAX_PATHNAME_SZ];
			mode_t mode;
		} chmod;
		
		struct
		{
			char pathname[MAX_PATHNAME_SZ];
			uid_t owner;
			gid_t group;
		} chown;
	
		struct
		{
			char pathname[MAX_PATHNAME_SZ];
		} remove;
	
		struct
		{
			char oldname[MAX_PATHNAME_SZ];
			char newname[MAX_PATHNAME_SZ];
		} rename;

		struct
		{
			off_t size;
		} ftruncate;
				
		struct
		{
			char pathname[MAX_PATHNAME_SZ];
			off_t size;
		} truncate;
		
		struct
		{
			struct stat stat;
		} fstat;
		
		struct
		{
			char pathname[MAX_PATHNAME_SZ];
			struct stat stat;
		} stat;
		
		struct
		{
			char pathname[MAX_PATHNAME_SZ];
			int amode;
		} access;
		
		struct
		{
			char volumename[MAX_PATHNAME_SZ];
			char newname[MAX_PATHNAME_SZ];
		} relabel;
		
		struct
		{
			char volumename[MAX_PATHNAME_SZ];
			char newname[MAX_PATHNAME_SZ];
			char filesystem[FILESYSTEMNAME_SZ];
			int block_sz;
			bits32_t flags;
		} format;
		
		struct
		{
			char volumename[MAX_PATHNAME_SZ];
			uint8 guid[16];
		}  getvolumeguid;
		
		struct
		{
			char volumename[MAX_PATHNAME_SZ];
			uint8 guid[16];
		}  setvolumeguid;
		
		struct
		{
			int index;
			struct DeviceEntry devent;
		}  readdeviceentry;
		
		struct
		{
			char devicename[DISKNAME_SZ];
			int nr_partition;
			struct Partition partition[NPARTITION];			
		}  readpartitiontable;
		
		struct
		{
			char devicename[DISKNAME_SZ];
			int nr_partition;
			struct Partition partition[NPARTITION];
		}  writepartitiontable;
		
		struct
		{
			char pathname[MAX_PATHNAME_SZ];
			char filename[MAX_PATHNAME_SZ];
		} setprogramdir;
		
		struct
		{
			size_t fpos;
			size_t nbytes;
		} segread;
		
		struct
		{
			size_t fpos;
			size_t nbytes;
		} segwrite;
		
		struct
		{
			uint64 cookie;
		} waitnotify;
		
		struct
		{
			int handle;		// client handle to duplicate
		} dup;
	} s;
	

	uint8 buf[FS_BUFFER_SZ];
	
	union
	{
		int close;
		off_t lseek;
		int fchdir;
		int rewinddir;
		int ftruncate;
		int installmbrbootloader;
		int setremovableattrs;
		int open;
		ssize_t read;
		ssize_t write;
		int chdir;
		int mkdir;
		int rmdir;
		int opendir;
		int readdir;
		int symlink;
		int chmod;
		int chown;
		int remove;
		int rename;
		int truncate;
		int fstat;
		int stat;
		int access;
		int relabel;
		int format;
		int getvolumeguid;
		int setvolumeguid;
		int readdeviceentry;
		int readpartitiontable;
		int writepartitiontable;
		int setprogramdir;
		ssize_t segread;
		ssize_t segwrite;
		uint64 startnotify;
		int endnotify;
		uint64 waitnotify;
		int dup;
	} r;
};




/*
 * Filesystem prototypes
 */


int relabel (char *volumename, char *name);
int format (char *volumename, char *name, char *filesystem, int block_sz, bits32_t flags);
int getvolumeguid (char *volumename, uint8 *guid);
int setvolumeguid (char *volumename, uint8 *guid);
int readdeviceentry (int idx, struct DeviceEntry *devent);
int readpartitiontable (char *devicename, struct Partition *pt, bits32_t flags);
int writepartitiontable (char *devicename, struct Partition *pt, bits32_t flags);
int installmbrbootloader (char *devicename, void *boot_img, void *core_img, size_t core_sz);

int setremovableattrs (uid_t uid, gid_t gid);
int setprogramdir (char *pathname, char *filename, int filename_sz);

int dup (int fd);
uint64 startnotify (int fd);
uint64 waitnotify (int fd, uint64 cookie);
int putnotify (int fd, uint64 cookie);
uint64 getnotify (int fd);
int endnotify (int fd);


#ifdef __cplusplus
}
#endif

#endif







#ifndef EXEC_FILESYSTEM_H
#define EXEC_FILESYSTEM_H

#include <interfaces/filesystem.h>
#include <interfaces/block.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/lists.h>
#include <sys/dirent.h>
#include <task.h>


// ***************************************************************************

#define PATH_MAX        256
#define NAME_MAX        256



// Table sizes

#define NR_FILEDESC             1024
#define NR_VNODE				512
#define NR_SUPERBLOCK			26
#define NR_BLOCKDEV				26
#define NR_MOUNT				26
#define NR_INFLIGHTBUF			16
#define NR_FILESYSTEM			3


// Directory Name Lookup Cache values

#define NR_DNAME				4096
#define DNAME_SZ				64
#define DNAME_HASH				128

// Limit of number of symlinks that can be followed

#define MAX_SYMLINK				32

// Lookup() commands
 
#define LOOKUP				0
#define LASTDIR				1

// Lookup() flags

#define FOLLOW				(1<<0)
#define NOFOLLOW			0


// IsAllowed() desired access flags

#define R_BIT	004
#define W_BIT	002
#define X_BIT	001


// File mode mask to use on removable devices, preventing setuid() use.

#define REMOVABLE_FILE_MASK		00777
#define REMOVABLE_DIR_MASK		00777


// Unmount() flush

#define UNMOUNT_NO_FLUSH		0
#define UNMOUNT_FLUSH			1




// ***************************************************************************

// Alignment macros
 
#define ALIGN_UP(val, alignment)								\
	(((val + (alignment) - 1)/(alignment))*(alignment))
	
#define ALIGN_DOWN(val, alignment)								\
			((val) - ((val) % (alignment)))



// ***************************************************************************

// Forward Declerations

// class VFS;
struct VNode;
class FileDesc;


// ***************************************************************************
// struct MBRPartitionEntry

struct MBRPartitionEntry
{
	uint8 status;
	uint8 chs[3];
	uint8 partition_type;
	uint8 chs_last[3];
	uint32 lba;
	uint32 nsectors;
} __attribute__ ((packed));



// ***************************************************************************
// blockdev.state

#define BDEV_STATE_MOUNTED		0
#define BDEV_STATE_UNMOUNTED	1

// blockdev.flags

#define BDF_ABORT				(1<<0)




// Struct BlockDev
 
struct BlockDev
{
	LIST_ENTRY (BlockDev) link;

	Rendez rendez;
	bool msg_pending;
	int task_id;
	int handle;
//	int state;
	int reference_cnt;
	uuid_t uuid;
	struct GenDisk gendisk;
	bits32_t flags;
		
	LIST (Buf) strategy_list;

	struct MsgBlock *msg;
	
	int partition_cnt;
//	struct Partition partition_table[NPARTITION];
};



// ***************************************************************************

// Buffer size and number of hash table entries

// #define BUF_SZ				4096
#define NR_HASH				128		// Hash table lookup
#define DELWRI_SECONDS		5		// Seconds for delay writes
#define DIRTY_HASH			64


// buf.flags

#define B_FREE						(1 << 0)		// On free list
#define B_VALID						(1 << 2)		// Valid, on lookup hash list
#define B_BUSY						(1 << 3)		// Busy

#define B_STRATEGY					(1 << 4)		// Shouldn't be needed, B_BUSY will be held
#define B_IODONE					(1 << 5)		// IO Complete

#define B_READ						(1 << 6)	
#define B_WRITE						(1 << 7)	
#define B_ASYNC						(1 << 8)

#define B_ERROR						(1 << 9)

#define B_DELWRI					(1 << 10)

#define B_WRITECLUSTER				(1 << 11)





// struct Cluster

struct Buf
{
	Rendez rendez;
	bits32_t flags;
	
	struct BlockDev *blockdev;
	uint64 cluster_nr;
	void *addr;
	void *cluster_buf;
	uint64 write_offset;
	uint64 write_size;
	
	LIST_ENTRY (Buf) free_link;				// Free Clean list
	LIST_ENTRY (Buf) lookup_link;			// Lookup hash list
	LIST_ENTRY (Buf) strategy_link;			// BlockDev's list of blocks to perform IO on
	
	struct TimeVal expire_time;
};



// ***************************************************************************

struct VAttr
{
	dev_t dev;
	mode_t mode;
	ino_t ino;
	uid_t uid;
	uid_t gid;
	nlink_t nlink;
	off_t size;
	time_t atime;
	time_t mtime;
	time_t ctime;
	long blksize;
	long blocks;
};



// ***************************************************************************

// class VFS

class VFS
{
	public: 
	
	const char *name;
	void *data;
	int priority;
	
	VFS(const char *name, void *data, int priority);
	~VFS();
	
	LIST_ENTRY (VFS) link;
	
	// VFS operations (superblock as constructor parameter ???)
	// Needs to be static classes, created for each file-system type, ISO9660, Ext2 etc.

	virtual int mount (struct SuperBlock *sb);
	virtual int unmount (struct SuperBlock *sb);
	
	virtual int relabel (struct SuperBlock *sb, char *newlabel, struct FileDesc *fd);
	virtual int format (struct SuperBlock *sb, char *newlabel, int block_size, bits32_t flags, struct FileDesc *fd);
	virtual int getvolumeguid (struct SuperBlock *sb, uint8 *volume_guid, struct FileDesc *fd);
	virtual int setvolumeguid (struct SuperBlock *sb, uint8 *rvolume_guid, struct FileDesc *fd);

	// VNode operations

	virtual int lookup (struct VNode *dir, char *name, struct VNode **result, struct FileDesc *fd);
	virtual int create (struct VNode *dir, char *name, struct VAttr *va, int flags, mode_t mode, struct VNode **vn, struct FileDesc *fd);
	virtual void close (struct VNode *vnode);
	
	virtual size_t read (struct VNode *vnode, void *buf, int64 *offset, size_t nbytes, struct FileDesc *fd);
	virtual size_t write (struct VNode *vnode, void *buf, int64 *offset, size_t nbytes, struct FileDesc *fd);
	
	virtual int remove (struct VNode *dir, char *name, struct FileDesc *fd);
	virtual int rename (struct VNode *sdir, char *sname, struct VNode *ddir, char *dname, struct FileDesc *fd);
	virtual int truncate (struct VNode *vnode, int64 size, struct FileDesc *fd);
	virtual int mkdir (struct VNode *dir, char *name, struct VAttr *va, struct VNode **vn, struct FileDesc *fd);
	virtual int rmdir (struct VNode *dir, char *name, struct FileDesc *fd);
	virtual int readdir (struct VNode *dir, struct dirent *dent, int64 *offset, struct FileDesc *fd);
	virtual int access (struct VNode *vnode, mode_t mode, struct FileDesc *fd);
	virtual int setattr (struct VNode *vnode, struct VAttr *va, struct FileDesc *fd);
	virtual int getattr (struct VNode *vnode, struct VAttr *va, struct FileDesc *fd);
};



// ***************************************************************************

// vnode.type

#define VNODE_TYPE_NONE					1
#define VNODE_TYPE_REG					2
#define VNODE_TYPE_DIR					3
#define VNODE_TYPE_SYMLINK				4
#define VNODE_TYPE_UNKNOWN				5
#define VNODE_TYPE_MOUNT				6

// vnode.flags

#define V_FREE							(1<<1)  
#define V_VALID							(1<<2)
#define V_ROOT							(1<<3)
#define V_ABORT							(1<<4)



// struct VNode

struct VNode
{
	Rendez rendez;
	int busy;
	
	int flags;		
	int type;
	int reference_cnt;
	class VFS *vfs;
	struct SuperBlock *superblock;
	ino_t inode_nr;					// inode number
	mode_t mode;					// file type, protection, etc
	uid_t uid;						// user id of the file's owner
	gid_t gid;						// group id
	off_t size;						// file size in bytes
	time_t atime;
	time_t mtime;
	time_t ctime;
	int64 cookie;			// Incremented each time a file or directory is modified
							// used to optimize readdir/lookup f_pos offsets
							// and for file/dir notification.

	LIST (Connection) notification_list;							

	void *data;				// per FS data such as in memory inode/
	LIST_ENTRY (VNode) hash_entry;
	LIST_ENTRY (VNode) vnode_entry;
	
	LIST (DName) vnode_list;		// List of all entries pointing to this vnode
	LIST (DName) directory_list;	// List of all entries within this directory
};



// ***************************************************************************

// superblock.flags

#define S_ABORT			(1<<0)
#define S_READONLY		(1<<1)

// struct SuperBlock

struct SuperBlock
{
	int dev_handle;
	Rendez rendez;
	
	uint64 start_sector;		// replace with start_sector
	uint64 nsectors;
	uint32 block_size;			// start sector needs to be aligned with block size
	
	class VFS *vfs;
	struct VNode *root;

	struct BlockDev *blockdev;
	int partition_idx;
	int flags;
	int reference_cnt;
	
//	char label[DISKNAME_SZ];
	
	void *data;
};



// ***************************************************************************

// mount.type

#define MOUNT_TYPE_FREE			0
#define MOUNT_TYPE_VOLUME		1
#define MOUNT_TYPE_SYMLINK		2




// FIXME: Rename to RootDirent
// struct Mount

struct Mount
{
	int type;
//	char name[NAME_MAX];
	struct SuperBlock *sb;
	char *link;
};



// ***************************************************************************
// struct DName

struct DName
{
	LIST_ENTRY (DName) lru_link;
	LIST_ENTRY (DName) hash_link;
	int hash_key;
	struct VNode *dir_vnode;
	struct VNode *vnode;
	char name[DNAME_SZ];
	
	LIST_ENTRY (DName) vnode_link;		// Hard links to vnode
	LIST_ENTRY (DName) directory_link;	// Vnodes within DName's directory
};






// class SystemDesc

class SystemDesc
{
    public:
    
    SystemDesc();
    ~SystemDesc();
    
	static void SystemDescTask (void *arg);
	int task_id;
	
	int handle;
	
	Rendez rendez;
	
	private:
	
	void setuid();
	void setgid();
	void getuid();
	void getgid();
};



// ***************************************************************************
// class FileDesc

class FileDesc
{
	public:
	
	FileDesc ();
	~FileDesc();
	
	static void FileDescTask (void *arg);
	int task_id;	


    int h;                  // handle  which is fd[1]
	int fd[2];
    	
	int uid;
	int gid;
	
	
	
	LIST_ENTRY (FileDesc) link;

    class FileDesc *root_filedesc;

	bool msg_pending;
	bool has_exited;
	int handle;
	int error;
	
//	struct VNode *current_directory;	// FIXME: Need to 
//	struct VNode *program_directory;	// FIXME: Need to initialize (and handle in Lookup) ':' for prog dir?
	
	struct VNode *vnode;			    	// vnode of file
	int64 offset;						// seek position

	bool validate_vnode;			// FIXME: What is this for?????????  ValidateFilp() ???????
	
	int64 cookie;
	LIST_ENTRY (Connection) notify_link;
	
	Rendez rendez;

	struct FileMsg *msg;
	void *msg_data;
	
	int IsAllowed (struct VNode *vnode, mode_t desired_access);
	int IsOwner (struct VNode *vnode);

	private:

//	struct Connection *FindConnection (int pid);
//	void ValidateVNode();
	void open();
	void close();
	void read();
	void write();
	void lseek();
	void chdir();
	void fchdir();
	void opendir();
	void readdir();
	void rewinddir();
	void mkdir();
	void rmdir();
	void remove();
	void rename();
	void fstat();
	void stat();
	void chmod();
	void chown();
	void ftruncate();
	void truncate();
	void unmount();
	void mount();
	void dup();
	void startnotify();
	void waitnotify();
	void endnotify();
};




// ***************************************************************************

LIST_TYPE (FileDesc) filedesc_list_t;
LIST_TYPE (BlockDev) blockdev_list_t;
LIST_TYPE (DName) dname_list_t;
LIST_TYPE (Buf) buf_list_t;
LIST_TYPE (VNode) vnode_list_t;
LIST_TYPE (VFS) vfs_list_t;







extern vfs_list_t vfs_list;

extern char boot_type;
extern char boot_name[NAME_MAX];
extern char syslink[NAME_MAX];
extern bool system_volume_found;

extern uid_t admin_uid;				// FIXME: Remove??????
extern gid_t admin_gid;

extern uid_t removable_uid;
extern gid_t removable_gid;

extern mode_t default_dir_mode;
extern mode_t default_file_mode;

extern filedesc_list_t filedesc_list;
extern blockdev_list_t blockdev_list;

extern Rendez automount_rendez;
extern int automount_task_id;
extern int portchange;

extern Rendez mounts_rendez;
extern int mounts_busy;

extern int nameport_handle;
//extern h_set handle_set, event_set;

extern int FileDesc_cnt;
extern int exit_status;
extern int request_join;

extern vnode_list_t vnode_free_list;

extern struct VNode root_vnode;
extern struct SuperBlock root_superblock;



extern struct Mount mount_table[NR_MOUNT];
extern struct SuperBlock superblock_table[NR_SUPERBLOCK];
extern struct VNode vnode_table[NR_VNODE];
extern class FileDesc *filedesc_table[NR_FILEDESC];
extern struct BlockDev blockdev_table[NR_BLOCKDEV];

extern struct DName dname_table[NR_DNAME];
extern dname_list_t dname_lru_list;
extern dname_list_t dname_hash[DNAME_HASH];

extern Rendez buf_list_rendez;
extern size_t cluster_size;
extern struct Buf *buf_table;
extern int nclusters;
extern int cache_fraction;
extern buf_list_t buf_hash[NR_HASH];
// extern int bufs_in_use;
extern buf_list_t buf_avail_list;

extern Rendez flush_rendez;
extern int flush_task_id;
extern int flush_timer;
extern int flush_request;

extern struct TimeVal current_time;
extern struct TimeVal flush_timeval;

// ***************************************************************************

/* automount.c */

void AutoMountTask (void *arg);
void UnmountDisconnectedDrives(void);
void UnmountAllDrives (void);
void MountNewDrives(void);
int Mount (uuid_t uuid, mode_t mode);
int Unmount (uuid_t uuid, int flush);
void UnmountPartition (struct SuperBlock *sb);
int MountPartitions (struct BlockDev *bdev);
int MountDrive (struct BlockDev *bdev);
struct SuperBlock *MountVolume (struct BlockDev *bdev, int partition);



/* buffer.c */

void BMap (struct SuperBlock *sb, uint64 fs_block, uint64 *block, int *offset);
int BDiskInfo (struct BlockDev *bdev);
struct Buf *GetBlk (struct BlockDev *bdev, uint64 cluster);
struct Buf *FindBlk (struct BlockDev *bdev, uint64 cluster);
struct Buf *BRead (struct BlockDev *bdev, uint64 cluster);
struct Buf *BRead (struct SuperBlock *sb, uint64 block);
void BRelse (struct Buf *buf);
int BWrite (struct Buf *buf);
int BAWrite (struct Buf *buf);
int BDWrite (struct Buf *buf);
void SyncBlockDev (struct BlockDev *bdev, int flush);
void Strategy (struct Buf *buf);
void StrategyTask(void *arg);
void FlushTask(void *arg);



/* init.c */

void FilesystemInit (int argc, char **argv);
void GetOptions (int argc, char **argv);
void InitBufferCache (void);
void InitBlockDevices (void);
void InitAutoMount(void);



/* partition.c, move into Automount */

int ReadPartitionTable (struct BlockDev *bdev);
uint8 *ReadSector (struct BlockDev *bdev, uint64 sector);

/* lookup.c */

struct VNode *Lookup (int cmd, char *path, char *r_last_component, int flags, struct FileDesc *cn);
char *GetName (char *remaining, char *component);
struct VNode *Advance (struct VNode *cur_vnode, char *component, struct FileDesc *cn);
int MergeLink (char *path, char *remaining, char *link);
int ReadPathname (struct FileDesc *cn, int path_sz, int path_offset, char *buf);
int DNameLookup (struct VNode *dir, char *name, struct VNode **vnp);
int DNameEnter (struct VNode *dir, struct VNode *vn, char *name);
int DNameRemove (struct VNode *dir, char *name);
int DNamePurgeVNode (struct VNode *vnode);
int DNamePurgeSuperblock (struct SuperBlock *sb);


/* vfs.c */

struct VNode *VNodeGet (struct SuperBlock *sb, int vnode_nr);
struct VNode *FindVNode (struct SuperBlock *sb, int vnode_nr);
void VNodeRelease (struct VNode *vnode);
void VNodeLock (struct VNode *vnode);
void VNodeUnlock (struct VNode *vnode);
void VNodeHold (struct VNode *vnode);
struct VNode *AllocVNode(void);
void FreeVNode(struct VNode *vnode);

struct SuperBlock *AllocSuperBlock (struct BlockDev *bdev);
void FreeSuperBlock (struct SuperBlock *sb);
struct BlockDev *AllocBlockDev(int handle, uuid_t *uuid);
void FreeBlockDev (struct BlockDev *bdev);
struct BlockDev *FindBlockDev (uuid_t *uuid);

/* volume.c */

int SystemVolumeCheck (struct SuperBlock *sb);
int CreateMountEntry (struct SuperBlock *sb);
struct Mount *AllocMount(void);
void FreeMount (struct Mount *mount);
struct Mount *FindMountByName (char *name);
















#endif


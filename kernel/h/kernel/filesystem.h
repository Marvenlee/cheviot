#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <dirent.h>
#include <fcntl.h>
#include <kernel/error.h>
#include <kernel/lists.h>
#include <kernel/msg.h>
#include <kernel/proc.h>
#include <kernel/timer.h>
#include <kernel/types.h>
#include <limits.h>
#include <sys/fsreq.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <poll.h>
#include <sys/syslimits.h>

#define BUFFER_SZ       1024          // FIXME: Used for queues ?

// Lookup() flags

#define LOOKUP_TYPE(x)  (x & 0x0000000f) // Lookup without parent or remove will return just matching vnode
#define LOOKUP_PARENT   (1 << 0) // Returns parent (and if exists the matching vnode)
#define LOOKUP_REMOVE   (2 << 0) // Returns both the parent and the vnode

#define LOOKUP_NOFOLLOW (1 << 5) // Don't follow last element
#define LOOKUP_KERNEL   (1 << 6)

// Table sizes
#define NR_SOCKET       1024
#define NR_SUPERBLOCK   128
#define NR_FILP         1024
#define NR_VNODE        128
#define NR_INFLIGHTBUF  16
#define NR_POLL         1024
#define NR_BUF          64
#define BUF_HASH        32
#define NR_PIPE         64

// Directory Name Lookup Cache values

#define NR_DNAME        64
#define DNAME_SZ        64
#define DNAME_HASH      32

// Limit of number of symlinks that can be followed

#define MAX_SYMLINK     32

// IsAllowed() desired access flags
// TODO: Needed ?
/*
#define R_BIT 004
#define W_BIT 002
#define X_BIT 001
*/


// Unmount() options
#define UNMOUNT_FORCE         (1 << 0)

// ***************************************************************************

// Buffer size and number of hash table entries

#define CACHE_PAGETABLES_CNT 256
#define CACHE_PAGETABLES_PDE_BASE 3072
#define CACHE_BASE_VA 0xC0000000
#define CACHE_CEILING_VA 0xD00000000

#define CLUSTER_SZ 0x10000
#define NBUF_HASH 128

#define DELWRI_SECONDS 5 // Seconds for delay writes
#define DIRTY_HASH 64

// buf.flags
#define B_FREE (1 << 0)  // On free list
#define B_VALID (1 << 2) // Valid, on lookup hash list
#define B_BUSY (1 << 3)  // Busy

#define B_STRATEGY (1 << 4) // Shouldn't be needed, B_BUSY will be held
#define B_IODONE (1 << 5)   // IO Complete

#define B_READ (1 << 6)
#define B_WRITE (1 << 7)
#define B_READDIR (1 << 8)

#define B_ASYNC (1 << 12)

#define B_ERROR (1 << 13)

#define B_DELWRI (1 << 14)

#define B_WRITECLUSTER (1 << 15)

#define B_DISCARD (1 << 16)

#define BUF_TYPE_PAGETABLE 1
#define BUF_TYPE_PAGEDIR 2
#define BUF_TYPE_FILE 3

#define BLK_DIR_READ 0
#define BLK_DIR_WRITE 1

// ReadQueue() and WriteQueue flags
#define Q_ALL (1 << 0)
#define Q_KERNEL (1 << 1)

// VAttr flags
#define VATTR_MODE (1 << 0)
#define VATTR_OWNER (1 << 1)
#define VATTR_ATIME (1 << 2)
#define VATTR_MTIME (1 << 3)
#define VATTR_CTIME (1 << 4)
#define VATTR_ALL (0xffffffff)

// vnode.flags

#define V_FREE (1 << 1)
#define V_VALID (1 << 2)
#define V_ROOT (1 << 3)
#define V_ABORT (1 << 4)

// superblock.flags

#define S_ABORT (1 << 0)
#define S_READONLY (1 << 1)

 
/*
 *
 */
 
#define PIPE_BUF_SZ                     1024
 

/*
 * Forward Declerations
 */
struct VNode;
struct Filp;
struct stat;
struct VFS;
struct DName;
struct InterruptAPI;    // TODO: Remove, run interrupt in user-mode, change API.


struct Poll {
  struct Process *process;
  struct VNode *vnode;
  int fd;
  short events;
  short revents;
  
  LIST_ENTRY(Poll) poll_link;
  LIST_ENTRY(Poll) vnode_link;
};


/*
 *
 */
struct Buf {
  struct Rendez rendez;
  bits32_t flags;

  struct VNode *vnode;
  struct BlockDev *blockdev;
  off64_t cluster_offset;
  size_t cluster_size;
  void *addr;

/*
  struct Rendez queue_rendez;
  uint8_t *queue_position;
  ssize_t queue_sz;
  ssize_t max_queue_sz;
  off64_t offset;
  off64_t remaining;
*/

/*
  off64_t cookie_first;
  off64_t cookie_next;
*/

  bits32_t dirty_bitmap; // For 32k cluster, means minimum 1k write

  LIST_ENTRY(Buf) free_link;   // Free Clean list
  LIST_ENTRY(Buf) lookup_link; // Lookup hash list

  LIST_ENTRY(Buf) delayed_write_link;
  
  struct Msg msg;       // Message header for when Buf written asynchronously
};


/*
 *
 */
struct Pipe
{
  struct Rendez rendez;

  LIST_ENTRY(Pipe) link;
  void *data;
  int w_pos;
  int r_pos;
  int free_sz;
  int data_sz;
  int reader_cnt;
  int writer_cnt;
};



/*
 *
 */
struct VNode {
  struct Rendez rendez;

  int busy;
  int reader_cnt;
  int writer_cnt;

  struct SuperBlock *superblock;

  int flags;
  int reference_cnt;

  struct VNode *vnode_covered;
  struct VNode *vnode_mounted_here;
  
  ino_t inode_nr; // inode number
  mode_t mode;    // file type, protection, etc
  uid_t uid;      // user id of the file's owner
  gid_t gid;      // group id
  off64_t size;   // file size in bytes
  time_t atime;
  time_t mtime;
  time_t ctime;
  int blocks;
  int blksize;
  int rdev;
  int nlink;


  // TODO: port Purpose here?
  //  struct MsgPort *msgport;


  LIST_ENTRY(VNode) msgport_link;
  LIST(Msg) msg_list;                 // why queue messages? because of single r/w/cmd per file?
  


  
  struct Pipe *pipe;


  LIST_ENTRY(VNode) hash_entry;
  LIST_ENTRY(VNode) vnode_entry;
  
  LIST(Poll) poll_list;

  // List of all entries pointing to this vnode
  LIST(DName) vnode_list;

  // List of all entries within this directory
  LIST(DName) directory_list;  

  uint32_t poll_events;
  struct Rendez poll_rendez;

  // Move interrupts out of file system.
  int isr_irq;
  void (*isr_callback)(int irq, struct InterruptAPI *api);
  struct Process *isr_process;
  LIST_ENTRY(VNode) isr_handler_entry;
};


/*
 *
 */
struct SuperBlock {
  struct Rendez rendez;
  bool busy;

  off64_t size;
  int block_size; // start sector needs to be aligned with block size

  struct VNode *server_vnode; // Might be useful?
  struct VNode *root; // Could replace with a flag to indicate root?  (what
                      // about rename path ascension?)

  uint32_t flags;
  int reference_cnt;

  void *data;
  LIST_ENTRY(SuperBlock) link;

  struct MsgPort msgport;

  int dev;

};

/*
 *
 */
struct DName {
  LIST_ENTRY(DName) lru_link;
  LIST_ENTRY(DName) hash_link;
  int hash_key;
  struct VNode *dir_vnode;
  struct VNode *vnode;
  char name[DNAME_SZ];

  LIST_ENTRY(DName) vnode_link;     // Hard links to vnode
  LIST_ENTRY(DName) directory_link; // Vnodes within DName's directory
};

/*
 *
 */
struct Filp {
  struct VNode *vnode;
  off64_t offset; // seek position
  int reference_cnt;
  LIST_ENTRY(Filp) filp_entry;
};

/*
 *
 */
struct Lookup {
  struct VNode *start_vnode;
  struct VNode *vnode;
  struct VNode *parent;
  char path[PATH_MAX];
  char *last_component;
  char *position;
  bool isLastName;
  int flags;
};

/*
 * FIXME: Move into IFS source?
 */
struct IFSHeader {
  char magic[4];
  uint32_t node_table_offset;
  int32_t node_cnt;
  uint32_t ifs_size;
} __attribute__((packed));

/*
 *
 */
struct IFSNode {
  char name[32];
  int32_t inode_nr;
  int32_t parent_inode_nr;
  uint32_t permissions;
  int32_t uid;
  int32_t gid;
  uint32_t file_offset;
  uint32_t file_size;
} __attribute__((packed));




// ***************************************************************************

LIST_TYPE(BlockDev) blockdev_list_t;
LIST_TYPE(DName) dname_list_t;
LIST_TYPE(Buf) buf_list_t;
LIST_TYPE(VNode) vnode_list_t;
LIST_TYPE(VFS) vfs_list_t;
LIST_TYPE(Filp) filp_list_t;
LIST_TYPE(SuperBlock) superblock_list_t;
LIST_TYPE(Poll) poll_list_t;
LIST_TYPE(Queue) queue_list_t;
LIST_TYPE(Pipe) pipe_list_t;

// Prototypes

// TODO: Move these somewhere
int Isatty(int fd);
int Pipe(int *fd);
int Fcntl(int fd, int cmd, int arg);

// fs/access.c
int Access(char *path, mode_t permisssions);
mode_t Umask (mode_t mode);
int Chmod(char *_path, mode_t mode);
int Chown(char *_path, uid_t uid, gid_t gid);
int IsAllowed(struct VNode *node, mode_t mode);

/* fs/cache.c */
struct Buf *AllocBuf(size_t cluster_size);
struct Buf *GetBlk(struct VNode *vnode, uint64_t cluster);
struct Buf *FindBlk(struct VNode *vnode, uint64_t cluster);
struct Buf *BRead(struct VNode *vnode, uint64_t cluster);
struct Buf *BReadDir(struct VNode *vnode, uint64_t cookie);
void BRelse(struct Buf *buf);
int BWrite(struct Buf *buf);
int BAWrite(struct Buf *buf);
int BDWrite(struct Buf *buf);
int BResize(struct Buf *buf, size_t sz);

int BSync (struct VNode *vnode);
//void SyncFile(struct VNode *vnode);
//void SyncBlockDev(struct BlockDev *bdev, bool flush);

void Strategy(struct Buf *buf);
void StrategyTask(void);
int SyncMount(char *path);
int Fsync(int fd);

/* fs/dir.c */
int Chdir(char *path);
int FChdir(int fd);
int Mkdir(char *pathname, mode_t mode);
int Rmdir(char *pathname);
int Opendir(char *name);
int Closedir(int fd);
int Readdir(int fd, struct dirent *dirent);
int Rewinddir(int fd);

/* fs/dnlc.c */
int DNameLookup(struct VNode *dir, char *name, struct VNode **vnp);
int DNameEnter(struct VNode *dir, struct VNode *vn, char *name);
int DNameRemove(struct VNode *dir, char *name);
void DNamePurgeVNode(struct VNode *vnode);
void DNamePurgeSuperblock(struct SuperBlock *sb);
void DNamePurgeAll(void);

/* fs/file.c */
int Open(char *_path, int oflags, mode_t mode);
int Kopen(char *_path, int oflags, mode_t mode);
int Truncate(int fd, size_t sz);
int Remove(char *_path);
int Rename(char *oldpath, char *newpath);
int Dup(int h);
int Dup2(int h, int new_h);
int Close(int h);
int Unlink(char *pathname);

/* fs/fs.c */

/* fs/handle.c */

//struct Filp *AllocFilp(void);
//void FreeFilp(struct Filp *filp);
struct Filp *GetFilp(int h);
int AllocHandle(void);
int FreeHandle(int h);
void InitProcessHandles(struct Process *proc);
void FreeProcessHandles(struct Process *proc);
int ForkProcessHandles(struct Process *newp, struct Process *oldp);
int CloseOnExecProcessHandles(void);

/* fs/init.c */
int InitVFS(void);

/* fs/lookup.c */
int Lookup(char *_path, int flags, struct Lookup *lookup);

/* fs/mount.c */
int ChRoot(char *_new_root);
int PivotRoot(char *_new_root, char *_old_root);
int MkNod(char *_handlerpath, uint32_t flags, struct stat *stat);
int Mount(char *_handlerpath, uint32_t flags, struct stat *stat);
int Unmount(int fd, bool force);
int MountRoot(uint32_t flags, struct stat *stat);

/* fs/poll.c */
int Poll (struct pollfd *pfds, nfds_t nfds, int timeout);
int PollNotify (int fd, int ino, short mask, short events);
void WakeupPolls(struct VNode *vnode, short mask, short events);
int PollNotifyFromISR(struct InterruptAPI *api, uint32_t mask, uint32_t events);

/* fs/queue.c */
struct Queue *AllocQueue(int type);
void FreeQueue(struct Queue *q);
ssize_t ReadQueue(struct Queue *q, void *_dst, size_t sz, int vmin, int vtime);
ssize_t WriteQueue(struct Queue *q, void *_src, size_t sz, int vmin, int vtime);

/* fs/rdwr.c */
ssize_t Read(int fd, void *buf, size_t count);
ssize_t Write(int fd, void *buf, size_t count);

ssize_t ReadFromChar (struct VNode *vnode, void *src,
                               size_t nbytes);
ssize_t ReadFromCache (struct VNode *vnode, void *src,
                               size_t nbytes, off64_t *offset);
ssize_t ReadFromFifo (struct VNode *vnode, void *src,
                               size_t nbytes);

ssize_t WriteToChar (struct VNode *vnode, void *src,
                               size_t nbytes);                               
ssize_t WriteToCache (struct VNode *vnode, void *src,
                               size_t nbytes, off64_t *offset);
ssize_t WriteToFifo (struct VNode *vnode, void *src,
                               size_t nbytes);

/* fs/seek.c */
off_t Seek(int fd, off_t pos, int whence);
int Seek64(int fd, off64_t *pos, int whence);

/* fs/superblock.c */
struct SuperBlock *AllocSuperBlock(void);
void FreeSuperBlock(struct SuperBlock *sb);
void LockSuperBlock(struct SuperBlock *sb);
void UnlockSuperBlock(struct SuperBlock *sb);

/* fs/socket.c */
int SocketPair(int fd[2]);
struct Socket *AllocSocket(void);
void FreeSocket(struct Socket *socket);


/* fs/vfs.c */
ssize_t vfs_read(struct VNode *vnode, void *buf,
                               size_t nbytes, off64_t *offset);
ssize_t vfs_write(struct VNode *vnode, void *buf, size_t nbytes,
                                off64_t *offset);
int vfs_readdir(struct VNode *vnode, void *buf, size_t bytes,
                              off64_t *cookie);
int vfs_lookup(struct VNode *dir, char *name,
                             struct VNode **result);

int vfs_create(struct VNode *dvnode, char *name, int oflags, struct stat *stat, struct VNode **result);
                             
int vfs_unlink(struct VNode *dvnode, char *name);
int vfs_truncate(struct VNode *vnode, size_t sz);

int vfs_mknod(struct VNode *dir, char *name, struct stat *stat,
                            struct VNode **result);
                            
int vfs_mklink(struct VNode *dvnode, char *name, char *link,
                             struct stat *stat);
int vfs_rdlink(struct VNode *vnode, char *buf, size_t sz);
int vfs_rmdir(struct VNode *dir, char *name);
int vfs_mkdir(struct VNode *dir, char *name, struct stat *stat,
                            struct VNode **result);
int vfs_rename(struct VNode *src_dvnode, char *src_name,
                             struct VNode *dst_dvnode, char *dst_name);
int vfs_chmod(struct VNode *vnode, mode_t mode);
int vfs_chown(struct VNode *vnode, uid_t uid, gid_t gid);

/* fs/vnode.c */
struct VNode *VNodeNew(struct SuperBlock *sb, int inode_nr);
struct VNode *VNodeGet(struct SuperBlock *sb, int vnode_nr);
void VNodePut(struct VNode *vnode);
void VNodeIncRef(struct VNode *vnode);
void VNodeDecRef(struct VNode *vnode);
void VNodeFree(struct VNode *vnode);
void VNodeLock(struct VNode *vnode);
void VNodeUnlock(struct VNode *vnode);
struct VNode *FindVNode(struct SuperBlock *sb, int inode_nr);

void ValidateVNode(struct VNode *vnode);


#endif

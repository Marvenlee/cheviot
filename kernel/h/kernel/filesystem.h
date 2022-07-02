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

// lookup() flags
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

// Size of buffers for args and environment variables used during exec 
#define MAX_ARGS_SZ 0x10000

/*
 *
 */
 
#define PIPE_BUF_SZ                     1024
 

/*
 * Forward Declerations
 */
struct VNode;
struct Filp;
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

  // TODO: port Purpose here, helps with freeing if message is needs to be aborted?
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
 * TODO: Vnode operations for each device type reg, dir, fifo, char, block
 * TODO: Replace switch statements in read, write, etc
 */
struct VNodeOps
{
    ssize_t (*vn_read)(struct VNode *vnode, void *buf, size_t nbytes, off64_t *offset);
    ssize_t (*vn_write)(struct VNode *vnode, void *buf, size_t nbytes, off64_t *offset);
    int (*vn_truncate)(struct VNode *vnode, size_t sz);

    int (*vn_readdir)(struct VNode *dvnode, void *buf, size_t bytes, off64_t *cookie);
    int (*vn_lookup)(struct VNode *dvnode, char *name, struct VNode **result);
    int (*vn_create)(struct VNode *dvnode, char *name, int oflags, struct stat *stat, struct VNode **result);                             
    int (*vn_unlink)(struct VNode *dvnode, char *name);  // vfs_remove ?
    int (*vn_mknod)(struct VNode *dvnode, char *name, struct stat *stat, struct VNode **result);                            
    int (*vn_mklink)(struct VNode *dvnode, char *name, char *link, struct stat *stat);
    int (*vn_rdlink)(struct VNode *vnode, char *buf, size_t sz);
    int (*vn_rmdir)(struct VNode *dvnode, char *name);
    int (*vn_mkdir)(struct VNode *dvnode, char *name, struct stat *stat, struct VNode **result);
    int (*vn_rename)(struct VNode *src_dvnode, char *src_name, struct VNode *dst_dvnode, char *dst_name);
    
    int (*vn_chmod)(struct VNode *vnode, mode_t mode);
    int (*vn_chown)(struct VNode *vnode, uid_t uid, gid_t gid);    

    int (*vn_ioctl)(struct VNode *vnode, int cmd, int val);   

    // ioctl
    // fcntl
    // poll
    // stat
};


struct VFSOps
{
  int (*vfs_mount)();
  int (*vfs_unmount)();
  int (*vfs_root)();
  int (*vfs_statvfs)();
  int (*vfs_sync)(struct SuperBlock *sb);
  int (*vfs_vget)(struct SuperBlock *sb, struct VNode **result);
  int (*vfs_format)(struct SuperBlock *sb);  // not needed:  Format by raw reads and writes to /dev/sda1fs
};

/*
 *
 */
struct Filp {
  struct VNode *vnode;
  off64_t offset;
  int reference_cnt;
  LIST_ENTRY(Filp) filp_entry;
};

/*
 *
 */
struct lookupdata {
  struct VNode *start_vnode;
  struct VNode *vnode;
  struct VNode *parent;
  char path[PATH_MAX];
  char *last_component;
  char *position;
  char separator;
  int flags;
};



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

// fs/access.c
SYSCALL int sys_access(char *path, mode_t permisssions);
SYSCALL mode_t sys_umask(mode_t mode);
SYSCALL int sys_chmod(char *_path, mode_t mode);
SYSCALL int sys_chown(char *_path, uid_t uid, gid_t gid);
int is_allowed(struct VNode *node, mode_t mode);

/* fs/cache.c */
ssize_t read_from_cache (struct VNode *vnode, void *src, size_t nbytes, off64_t *offset, bool inkernel);
ssize_t write_to_cache (struct VNode *vnode, void *src, size_t nbytes, off64_t *offset);
struct Buf *alloc_buf(size_t cluster_size);
struct Buf *getblk(struct VNode *vnode, uint64_t cluster);
struct Buf *findblk(struct VNode *vnode, uint64_t cluster);
struct Buf *bread(struct VNode *vnode, uint64_t cluster);
struct Buf *breaddir(struct VNode *vnode, uint64_t cookie);
void brelse(struct Buf *buf);
int bwrite(struct Buf *buf);
int bawrite(struct Buf *buf);
int bdwrite(struct Buf *buf);
int bresize(struct Buf *buf, size_t sz);
int bsync(struct VNode *vnode);
void strategy(struct Buf *buf);
void strategy_task(void);
SYSCALL int sys_sync(void);
SYSCALL int sys_fsync(int fd);

/* fs/char.c */
SYSCALL int sys_isatty(int fd);
ssize_t read_from_char(struct VNode *vnode, void *src, size_t nbytes);
ssize_t write_to_char(struct VNode *vnode, void *src, size_t nbytes);                               

/* fs/dir.c */
SYSCALL int sys_chdir(char *path);
SYSCALL int sys_fchdir(int fd);
SYSCALL int sys_mkdir(char *pathname, mode_t mode);
SYSCALL int sys_rmdir(char *pathname);
SYSCALL int sys_opendir(char *name);
SYSCALL int sys_closedir(int fd);
SYSCALL ssize_t sys_readdir(int fd, void *dst, size_t sz);
SYSCALL int sys_rewinddir(int fd);

/* fs/dnlc.c */
int dname_lookup(struct VNode *dir, char *name, struct VNode **vnp);
int dname_enter(struct VNode *dir, struct VNode *vn, char *name);
int dname_remove(struct VNode *dir, char *name);
void dname_purge_vnode(struct VNode *vnode);
void dname_purge_superblock(struct SuperBlock *sb);
void dname_purge_all(void);

/* fs/exec.h */
SYSCALL int sys_exec(char *filename, struct execargs *args);
int copy_in_argv(char *pool, struct execargs *_args, struct execargs *args);
int copy_out_argv(void *stack_pointer, int stack_size, struct execargs *args);
char *alloc_arg_pool(void);
void free_arg_pool(char *mem);

/* fs/handle.c */
SYSCALL int sys_fcntl(int fd, int cmd, int arg);
SYSCALL int sys_dup(int h);
SYSCALL int sys_dup2(int h, int new_h);
SYSCALL int sys_close(int h);
struct Filp *get_filp(int fd);
int alloc_fd(void);
int free_fd(int fd);
void init_process_fds(struct Process *proc);
void close_process_fds(struct Process *proc);
int fork_process_fds(struct Process *newp, struct Process *oldp);
int close_on_exec_process_fds(void);

/* fs/init.c */
int InitVFS(void);

/* fs/link.c */
SYSCALL int sys_unlink(char *pathname);

/* fs/lookup.c */
int lookup(char *_path, int flags, struct lookupdata *ld);

/* fs/mount.c */
SYSCALL int sys_chroot(char *_new_root);
SYSCALL int sys_pivotroot(char *_new_root, char *_old_root);
SYSCALL int sys_movemount(char *_new_mount, char *old_mount);
SYSCALL int sys_mknod(char *_handlerpath, uint32_t flags, struct stat *stat);
SYSCALL int sys_mount(char *_handlerpath, uint32_t flags, struct stat *stat);
SYSCALL int sys_unmount(int fd, bool force);

/* fs/open.c */
SYSCALL int sys_open(char *_path, int oflags, mode_t mode);
SYSCALL int kopen(char *_path, int oflags, mode_t mode);

/* fs/pipe.c */
void InitPipes(void);
struct Pipe *AllocPipe(void);
void FreePipe(struct Pipe *pipe);
SYSCALL int sys_pipe(int _fd[2]);
ssize_t read_from_pipe (struct VNode *vnode, void *src, size_t nbytes);
ssize_t write_to_pipe (struct VNode *vnode, void *src, size_t nbytes);

/* fs/poll.c */
SYSCALL int sys_poll (struct pollfd *pfds, nfds_t nfds, int timeout);
SYSCALL int sys_pollnotify (int fd, int ino, short mask, short events);
void wakeup_polls(struct VNode *vnode, short mask, short events);
int PollNotifyFromISR(struct InterruptAPI *api, uint32_t mask, uint32_t events);

/* fs/truncate.c */
SYSCALL int sys_truncate(int fd, size_t sz);

/* fs/read.c */
SYSCALL ssize_t sys_read(int fd, void *buf, size_t count);
ssize_t kread(int fd, void *dst, size_t sz);

/* fs/rename.c */
SYSCALL int sys_rename(char *oldpath, char *newpath);

/* fs/seek.c */
SYSCALL off_t sys_lseek(int fd, off_t pos, int whence);
SYSCALL int sys_lseek64(int fd, off64_t *pos, int whence);

/* fs/superblock.c */
struct SuperBlock *AllocSuperBlock(void);
void FreeSuperBlock(struct SuperBlock *sb);
void LockSuperBlock(struct SuperBlock *sb);
void UnlockSuperBlock(struct SuperBlock *sb);

/* fs/socket.c */
SYSCALL int sys_socketpair(int fd[2]);
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
struct VNode *vnode_new(struct SuperBlock *sb, int inode_nr);
struct VNode *vnode_get(struct SuperBlock *sb, int vnode_nr);
void vnode_put(struct VNode *vnode);
void vnode_inc_ref(struct VNode *vnode);    // Why not just vnode->ref_cnt++;
void vnode_free(struct VNode *vnode);      // Delete a vnode from cache and disk

void vnode_lock(struct VNode *vnode);      // Acquire busy lock
void vnode_unlock(struct VNode *vnode);    // Release busy lock

/* fs/write.c */
SYSCALL ssize_t sys_write(int fd, void *buf, size_t count);



#endif

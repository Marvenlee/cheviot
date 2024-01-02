#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <kernel/arch.h>
#include <dirent.h>
#include <fcntl.h>
#include <kernel/error.h>
#include <kernel/lists.h>
#include <kernel/msg.h>
#include <kernel/timer.h>
#include <kernel/types.h>
#include <kernel/kqueue.h>
#include <kernel/types.h>
#include <limits.h>
#include <sys/fsreq.h>
#include <sys/stat.h>
#include <sys/termios.h>
#include <sys/execargs.h>
#include <unistd.h>
#include <sys/types.h>
#include <poll.h>
#include <sys/syslimits.h>
#include <sys/select.h>


// Forward declarations
struct VNode;
struct Filp;
struct VFS;
struct DName;
struct InterruptAPI;    // TODO: Remove, run interrupt in user-mode, change API.
struct Buf;
struct Msgport;
struct ISRHandler;
struct KQueue;

// List types
LIST_TYPE(Buf, buf_list_t, buf_link_t);
LIST_TYPE(DName, dname_list_t, dname_link_t);
LIST_TYPE(VNode, vnode_list_t, vnode_link_t);
LIST_TYPE(VFS, vfs_list_t, vfs_link_t);
LIST_TYPE(Filp, filp_list_t, filp_link_t);
LIST_TYPE(SuperBlock, superblock_list_t, superblock_link_t);
LIST_TYPE(Pipe, pipe_list_t, pipe_link_t);
LIST_TYPE(DelWriMsg, delwrimsg_list_t, delwrimsg_link_t);


// lookup() flags
#define LOOKUP_TYPE(x)    (x & 0x0000000f)  // Lookup without parent or remove will return just matching vnode
#define LOOKUP_PARENT     (1 << 0)  // Returns parent (and if exists the matching vnode)
#define LOOKUP_REMOVE     (2 << 0)  // Returns both the parent and the vnode

#define LOOKUP_NOFOLLOW   (1 << 5)  // Don't follow last element
#define LOOKUP_KERNEL     (1 << 6)

// Unmount() flags
#define UNMOUNT_FORCE     (1 << 0)

// set_fd() flags
#define FD_FLAG_CLOEXEC   (1 << 0)


// Sizes
#define MAX_SYMLINK     32     // Limit of number of symlinks that can be followed

#define NR_DNAME        64     // Number of entries in directory name lookup cache (DNLC)
#define DNAME_SZ        64
#define DNAME_HASH      32

#define NR_DELWRI_BUCKETS  (PAGE_SIZE / sizeof(buf_list_t))
#define NR_DELWRIMSG_PER_SB  (PAGE_SIZE / sizeof(struct DelWriMsg))

// Static table sizes (TODO: Adjust based on RAM size)
#define NR_SOCKET       1024
#define NR_SUPERBLOCK   128
#define NR_FILP         1024
#define NR_VNODE        1024
#define BUF_HASH        32
#define NR_PIPE         64
#define NR_BUF          1024    // Dynamically allocate ?
#define NR_MSGID2MSG    256     // Must match NPROCESS or greater

// Buffer size and number of hash table entries
#define CACHE_PAGETABLES_CNT        256
#define CACHE_PAGETABLES_PDE_BASE   3072
#define CACHE_BASE_VA               0xC0000000
#define CACHE_CEILING_VA            0xD0000000

#define CLUSTER_SZ      4096        // TODO: Enlarge back to 16K ?
#define NBUF_HASH       128

#define MAX_ARGS_SZ     0x10000   // Size of buffers for args and environment variables used during exec 

#define PIPE_BUF_SZ     4096      // Buffer size of pipes (should be same as page size)

#define BDFLUSH_WAKEUP_INTERVAL_MS    300  // Period between runs of the bd_flush task
#define BDFLUSH_SOFTCLOCK_TICKS       50   // time between buckets on delayed-write timing wheels
#define DELWRI_DELAY_TICKS            500  // Time in ticks to delay a delayed-write


/* @brief   A single cluster of a file in the file buffer cache
 */
struct Buf
{  
  struct Rendez rendez;
  bits32_t flags;
  struct VNode *vnode;

  off64_t cluster_offset;
  void *data;                     // Address of the page-sized buffer holding cached file data

  buf_link_t free_link;           // Free list entry
  buf_link_t lookup_link;         // Hash table entry
  buf_link_t delwri_hash_link;    // Delayed write entry  
  buf_link_t vnode_link;          // Entry on vnode's list of cached blocks

  uint64_t expiration_time;       // Time that a delayed-write block should be flushed
};

// Buf.flags
#define B_FREE      (1 << 0)  // On free list
#define B_VALID     (1 << 2)  // Valid, on lookup hash list
#define B_BUSY      (1 << 3)  // Busy
#define B_ERROR     (1 << 4)  // Buf is not valid (discarded in brelse)
#define B_DISCARD   (1 << 5)  // Discard block in brelse



// FIXME: Do we need the ones below, can these be hints to vfs_strategy?
#define B_READ      (1 << 6)  // Read block of file (upto block size, depending om file size)
#define B_WRITE     (1 << 7)  // Write block of file (upto block size, depending on file size)
#define B_ASYNC     (1 << 8)  // Hint to FS Handler that block won't be written again soon (writing next block).
#define B_DELWRI    (1 << 9)  // Hint to FS Handler that block may be written again soon
#define B_READAHEAD (1 << 10)  // Hint to FS Handler to read additional blocks after this block has been read.


/* @brief   Message and metadata for handling delayed writes by the bdflush_task
 * 
 */
struct DelWriMsg
{
  struct SuperBlock *superblock;
  struct Msg msg;
  struct fsreq req;
  struct fsreply reply;
  struct IOV siov[2];
  struct IOV riov[1];
  struct Buf *buf;  
  delwrimsg_link_t link;
};


/* @brief   Pipe state
 */
struct Pipe
{
  struct Rendez rendez;
  pipe_link_t link;
  void *data;
  int w_pos;
  int r_pos;
  int free_sz;
  int data_sz;
  int reader_cnt;     // Number of filps that are readers (not total FDs?)
  int writer_cnt;     // Number of filps that are writers (not total FDs?)
};


/* @brief   VNode state representing a file, directory, pipe or special device.
 */
struct VNode
{
  struct Rendez rendez;

  int busy;           // For locking fields of VNode
  int reader_cnt;     // For read/write access of character devices
  int writer_cnt;     // For read/write access of character devices

  struct SuperBlock *superblock;

  int flags;
  int reference_cnt;

  struct VNode *vnode_covered;
  struct VNode *vnode_mounted_here;
  
  struct Pipe *pipe;      // FIXME Perhaps a union of different specialist objects?
                          // Will we need something for sockets and socketpair ?
                          // Or will we have a FS handler for those?
  
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
  
  vnode_link_t hash_entry;
  vnode_link_t vnode_entry;
  
  buf_list_t buf_list;
  
  dname_list_t vnode_list;          // List of all dname entries pointing to this vnode
  dname_list_t directory_list;      // List of all entries within this directory
  knote_list_t knote_list;
};

// VNode.flags
#define V_FREE (1 << 1)
#define V_VALID (1 << 2)
#define V_ROOT (1 << 3)
#define V_ABORT (1 << 4)


/* @brief   SuperBlock data structure for a mounted filesystem.
 */
struct SuperBlock
{
  struct Rendez rendez;
  bool busy;

  int dev;            // FIXME: Unique major/minor id?

  struct MsgPort msgport;

  off64_t size;
  int block_size;     // start sector needs to be aligned with block size

  struct VNode *root; // Could replace with a flag to indicate root?  (what
                      // about rename path ascension?)
  uint32_t flags;
  int reference_cnt;

  superblock_link_t link;
  superblock_link_t writable_filesystem_link;  // Filesystems that have delayed-write support.
    
  buf_list_t *delwri_timing_wheel;    // Timing wheel for writing async and delayed write bufs       
  delwrimsg_list_t free_delwri_msg_list;
  struct DelWriMsg *delwrimsg_pool;
  uint64_t softclock;

};

// SuperBlock.flags
#define S_ABORT     (1 << 0)
#define S_READONLY  (1 << 1)


/* @brief   Entry in the Directory Name Lookup Cache (DNLC)
 */
struct DName {
  struct VNode *dir_vnode;
  struct VNode *vnode;
  char name[DNAME_SZ];
  int hash_key;
  
  dname_link_t lru_link;
  dname_link_t hash_link;
  dname_link_t vnode_link;      // Hard links to vnode
  dname_link_t directory_link;  // Vnodes within DName's directory
};


/* @brief   File pointer of an open file
 */
struct Filp {
  int type;
  
  union {
    struct VNode *vnode;
    struct SuperBlock *superblock;
    struct KQueue *kqueue;
    struct ISRHandler *isrhandler;
  } u;
  
  off64_t offset;
  mode_t mode;        // TODO: Need to set the access mode on file open or fcntl
  uint32_t flags;     // Access flags, e.g. O_READ, O_WRITE
  int reference_cnt;
  filp_link_t filp_entry;   // VNode link
};

// Filp.type
#define FILP_TYPE_UNDEF        0
#define FILP_TYPE_VNODE        1
#define FILP_TYPE_SUPERBLOCK   2
#define FILP_TYPE_KQUEUE       3
#define FILP_TYPE_ISRHANDLER   4
#define FILP_TYPE_PIPE         5
#define FILP_TYPE_SOCKET       6
#define FILP_TYPE_SOCKETPAIR   7


/* @brief   Management of a process's file descriptors
 */
struct FProcess
{
  struct Filp *fd_table[OPEN_MAX];          // OPEN_MAX and FD_SETSIZE should be equal
  fd_set fd_close_on_exec_set;        // Newlib defines size of 
  fd_set fd_in_use_set;
  
  mode_t umask;
  struct VNode *current_dir;
  struct VNode *root_dir;
};


/* @brief   Lookup state of files to vnodes
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


/*
 * Prototypes
 */
// fs/access.c
int sys_access(char *path, mode_t permisssions);
mode_t sys_umask(mode_t mode);
int sys_chmod(char *_path, mode_t mode);
int sys_chown(char *_path, uid_t uid, gid_t gid);
int is_allowed(struct VNode *node, mode_t mode);

// fs/block.c
ssize_t read_from_block (struct VNode *vnode, void *dst, size_t sz, off64_t *offset);
ssize_t write_to_block (struct VNode *vnode, void *dst, size_t sz, off64_t *offset);

/* fs/cache.c */
ssize_t read_from_cache (struct VNode *vnode, void *src, size_t nbytes, off64_t *offset, bool inkernel);
ssize_t write_to_cache (struct VNode *vnode, void *src, size_t nbytes, off64_t *offset);
struct Buf *bread(struct VNode *vnode, uint64_t cluster);
int bwrite(struct Buf *buf);
int bawrite(struct Buf *buf);
int bdwrite(struct Buf *buf);
void brelse(struct Buf *buf);
struct Buf *getblk(struct VNode *vnode, uint64_t cluster);
struct Buf *findblk(struct VNode *vnode, uint64_t cluster);
int btruncate(struct VNode *vnode);
size_t resize_cache(size_t free);
int init_superblock_bdflush(struct SuperBlock *sb);
void deinit_superblock_bdflush(struct SuperBlock *sb);
int bdflush(void *arg);
struct Buf *find_delayed_write_buf(struct SuperBlock *sb, uint64_t softclock);
void unhash_delayed_write_buf(struct SuperBlock *sb, uint64_t softclock, struct Buf *buf);
struct DelWriMsg *alloc_delwri_msg(struct SuperBlock *sb);
void free_delwri_msg(struct SuperBlock *sb, struct DelWriMsg *msg);

/* fs/char.c */
int sys_isatty(int fd);
ssize_t read_from_char(struct VNode *vnode, void *src, size_t nbytes);
ssize_t write_to_char(struct VNode *vnode, void *src, size_t nbytes);                               

/* fs/dir.c */
int sys_chdir(char *path);
int sys_fchdir(int fd);
int sys_chroot(char *_new_root);
int sys_opendir(char *name);
int sys_closedir(int fd);
int sys_mkdir(char *pathname, mode_t mode);
int sys_rmdir(char *pathname);
ssize_t sys_readdir(int fd, void *dst, size_t sz);
int sys_rewinddir(int fd);

/* fs/dnlc.c */
int dname_lookup(struct VNode *dir, char *name, struct VNode **vnp);
int dname_enter(struct VNode *dir, struct VNode *vn, char *name);
int dname_remove(struct VNode *dir, char *name);
void dname_purge_vnode(struct VNode *vnode);
void dname_purge_superblock(struct SuperBlock *sb);
void dname_purge_all(void);

/* fs/exec.c */
int sys_exec(char *filename, struct execargs *args);
int copy_in_argv(char *pool, struct execargs *_args, struct execargs *args);
int copy_out_argv(void *stack_pointer, int stack_size, struct execargs *args);
char *alloc_arg_pool(void);
void free_arg_pool(char *mem);

/* fs/filedesc.c */
int sys_fcntl(int fd, int cmd, int arg);
int sys_dup(int h);
int sys_dup2(int h, int new_h);
int sys_close(int h);
int do_close(struct Process *proc, int fd);

int dup_fd(struct Process *proc, int fd, int min_fd, int max_fd);

int init_fproc(struct Process *proc);
int fini_fproc(struct Process *proc);
int fork_process_fds(struct Process *newp, struct Process *oldp);
int close_on_exec_process_fds(void);

int alloc_fd(struct Process *proc, int min_fd, int max_fd);
int free_fd(struct Process *proc, int fd);
int set_fd(struct Process *proc, int fd, int type, uint32_t flags, void *item);

/* fs/filp.c */
int close_filp(struct Process *proc, int fd);
struct Filp *get_filp(struct Process *proc, int fd);
int alloc_fd_filp(struct Process *proc);
int free_fd_filp(struct Process *proc, int fd);
struct Filp *alloc_filp(void);
void free_filp(struct Filp *filp);

/* fs/init.c */
int init_vfs(void);

/* fs/link.c */
int sys_unlink(char *pathname);

/* fs/lookup.c */
int lookup(char *_path, int flags, struct lookupdata *ld);
int init_lookup(char *_path, uint32_t flags, struct lookupdata *ld);
int lookup_path(struct lookupdata *ld);
int lookup_last_component(struct lookupdata *ld);
char *path_token(struct lookupdata *ld);
bool is_last_component(struct lookupdata *ld);
int walk_component (struct lookupdata *ld);

/* fs/mount.c */
int sys_pivotroot(char *_new_root, char *_old_root);
int sys_movemount(char *_new_mount, char *old_mount);
int sys_mknod(char *_handlerpath, uint32_t flags, struct stat *stat);
int sys_mount(char *_handlerpath, uint32_t flags, struct stat *stat);

int close_mount(struct Process *proc, int fd);
struct SuperBlock *get_superblock(struct Process *proc, int fd);
int alloc_fd_superblock(struct Process *proc);
int free_fd_superblock(struct Process *proc, int fd);
struct SuperBlock *alloc_superblock(void);
void free_superblock(struct SuperBlock *sb);

/* fs/open.c */
int sys_open(char *_path, int oflags, mode_t mode);
int kopen(char *_path, int oflags, mode_t mode);

/* fs/pipe.c */
void InitPipes(void);
struct Pipe *AllocPipe(void);
void FreePipe(struct Pipe *pipe);
int sys_pipe(int _fd[2]);
ssize_t read_from_pipe (struct VNode *vnode, void *src, size_t nbytes);
ssize_t write_to_pipe (struct VNode *vnode, void *src, size_t nbytes);

/* fs/poll.c */
int sys_poll (struct pollfd *pfds, nfds_t nfds, int timeout);
int sys_pollnotify (int fd, int ino, short mask, short events);

/* fs/truncate.c */
int sys_truncate(int fd, size_t sz);

/* fs/read.c */
ssize_t sys_read(int fd, void *buf, size_t count);
ssize_t kread(int fd, void *dst, size_t sz);

/* fs/rename.c */
int sys_rename(char *oldpath, char *newpath);

/* fs/seek.c */
off_t sys_lseek(int fd, off_t pos, int whence);
int sys_lseek64(int fd, off64_t *pos, int whence);

/* fs/socket.c */
int sys_socketpair(int fd[2]);
struct Socket *AllocSocket(void);
void FreeSocket(struct Socket *socket);

/* fs/sync.c */
int sys_sync(void);
int sys_fsync(int fd);

/* fs/vfs.c */
ssize_t vfs_read(struct VNode *vnode, void *buf, size_t nbytes, off64_t *offset);
ssize_t vfs_write(struct VNode *vnode, void *buf, size_t nbytes, off64_t *offset);
int vfs_write_async(struct SuperBlock *sb, struct DelWriMsg *msg, struct Buf *buf);
int vfs_readdir(struct VNode *vnode, void *buf, size_t bytes, off64_t *cookie);
int vfs_lookup(struct VNode *dir, char *name, struct VNode **result);
int vfs_create(struct VNode *dvnode, char *name, int oflags, struct stat *stat, struct VNode **result);                             
int vfs_unlink(struct VNode *dvnode, char *name);
int vfs_truncate(struct VNode *vnode, size_t sz);
int vfs_mknod(struct VNode *dir, char *name, struct stat *stat, struct VNode **result);                            
int vfs_mklink(struct VNode *dvnode, char *name, char *link, struct stat *stat);
int vfs_rdlink(struct VNode *vnode, char *buf, size_t sz);
int vfs_rmdir(struct VNode *dir, char *name);
int vfs_mkdir(struct VNode *dir, char *name, struct stat *stat, struct VNode **result);
int vfs_rename(struct VNode *src_dvnode, char *src_name, struct VNode *dst_dvnode, char *dst_name);
int vfs_chmod(struct VNode *vnode, mode_t mode);
int vfs_chown(struct VNode *vnode, uid_t uid, gid_t gid);
int vfs_fsync(struct VNode *vnode);

/* fs/vnode.c */
struct VNode *get_fd_vnode(struct Process *proc, int fd);
int close_vnode(struct Process *proc, int fd);
struct VNode *vnode_new(struct SuperBlock *sb, int inode_nr);
struct VNode *vnode_get(struct SuperBlock *sb, int vnode_nr);
void vnode_put(struct VNode *vnode);
void vnode_inc_ref(struct VNode *vnode);    // Why not just vnode->ref_cnt++;
void vnode_free(struct VNode *vnode);      // Delete a vnode from cache and disk

void vnode_lock(struct VNode *vnode);      // Acquire busy lock
void vnode_unlock(struct VNode *vnode);    // Release busy lock

/* fs/write.c */
ssize_t sys_write(int fd, void *buf, size_t count);


// Static asserts of the file system
// STATIC_ASSERT(sizeof(FileDescManager) <= PAGE_SIZE, "struct FileDescManager larger than page");
// STATIC_ASSERT(OPEN_MAX == FD_SETSIZE, "OPEN_MAX != FD_SETSIZE");

#endif

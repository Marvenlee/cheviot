#include <sys/syscalls.h>
#include <sys/lists.h>
#include <sys/debug.h>
#include <interfaces/block.h>
#include <interfaces/filesystem.h>
#include "filesystem.h"
//#include "fs_iso9660/iso9660.h"
//#include "fs_ext2/ext2.h"


int request_join;
vfs_list_t vfs_list;

char boot_type = 't';  	//	'l' = label,
						//	't' = type, "hd", "cd", 
						//	'p' = drive/partition, "atahda1"
char boot_name[NAME_MAX] = {"hd"};
char syslink[NAME_MAX];
bool system_volume_found;


uid_t admin_uid = 0;
gid_t admin_gid = 0;
uid_t removable_uid = 99;
gid_t removable_gid = 99;
mode_t default_dir_mode = 0755;
mode_t default_file_mode = 0644;

filedesc_list_t filedesc_list;
blockdev_list_t blockdev_list;

Rendez automount_rendez;
int automount_task_id;

Rendez mounts_rendez;
int mounts_busy;


// int nameport_handle;
//h_set handle_set, event_set;

int connection_cnt = 0;
int exit_status = 0;

vnode_list_t vnode_free_list;

struct VNode root_vnode;
struct SuperBlock root_superblock;

struct Mount mount_table[NR_MOUNT];
struct SuperBlock superblock_table[NR_SUPERBLOCK];
struct VNode vnode_table[NR_VNODE];
class FileDesc *filedesc_table[NR_FILEDESC];
struct BlockDev blockdev_table[NR_BLOCKDEV];

struct DName dname_table[NR_DNAME];
dname_list_t dname_lru_list;
dname_list_t dname_hash[DNAME_HASH];


size_t cluster_size;
int cache_fraction = 16;
int nclusters;
Rendez buf_list_rendez;
struct Buf *buf_table;
buf_list_t buf_hash[NR_HASH];
buf_list_t buf_avail_list;

Rendez flush_rendez;
int flush_task_id;
int flush_timer;
struct TimeVal current_time;
struct TimeVal flush_timeval;

int flush_request;
int portchange;


const char *mount_suffix[21] =
{
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
	"10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
	NULL
};












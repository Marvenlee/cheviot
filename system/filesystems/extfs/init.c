/* This file handles the ExtFS handler's initialization.
 *
 * Created (CheviotOS Filesystem Handler based)
 *   December 2023 (Marven Gilhespie) 
 */


#define LOG_LEVEL_INFO

#include <stdint.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <unistd.h>
#include "ext2.h"
#include "globals.h"

/* @brief   Initialize and mount an Ext filesystem
 *
 */
void init(int argc, char *argv[])
{
	struct stat blk_stat;
  struct stat mnt_stat;
  int sc;
  struct superblock *sp;
  
  log_info("ext2fs: init");
  
  memset (&superblock, 0, sizeof superblock);
  
  determine_cpu_endianness();
  
  sc = process_args(argc, argv);
  if (sc != 0) {
    panic("ext2fs failed to process command line arguments");
  }

  block_fd = open(config.device_path, O_RDWR);

  if (block_fd == -1) {  
    panic("ext2fs failed to open block device");
  }

  log_info("ext2fs: opened block device");

  // FIXME: Save the partition size returned by block stat, compare to
  // FIXME: Save the st_dev of the block device, this FS will be mounted with same value  
  
  if (fstat(block_fd, &blk_stat) != 0) {
     log_warn("ext2fs fstart failed");
  }
  
  if (read_superblock() != 0) {
    panic("ext2fs failed to read superblock");
  }

  log_info("ext2fs: read superblock");
  
  if ((cache = init_block_cache(block_fd, NR_CACHE_BLOCKS, sb_block_size)) == NULL) {
    panic("ext2fs init block cache failed");
  }

  if (init_inode_cache() != 0) {
    panic("ext2fs init inode cache failed");
  }
  
  mnt_stat.st_dev = blk_stat.st_dev;
  mnt_stat.st_ino = EXT2_ROOT_INO;
  mnt_stat.st_mode = S_IFDIR | (config.mode & 0777);
  mnt_stat.st_uid = config.uid;
  mnt_stat.st_gid = config.gid;
  mnt_stat.st_size = 0xFFFFFF00;  // FIXME: Get the size from partition or superblock
  mnt_stat.st_blksize = 512;
  mnt_stat.st_blocks = superblock.s_blocks_count; // Size in 512 byte blocks
  
  portid = createmsgport(config.mount_path, 0, &mnt_stat, NMSG_BACKLOG);

  if (portid == -1) {
    panic("ext2fs mounting failed");
  }

  kq = kqueue();
  
  if (kq == -1) {
    panic("ext2fs kqueue failed");
  }  
}


/* @brief   Handle command line arguments
 *
 * @param   argc, number of command line arguments
 * @param   argv, array of pointers to command line argument strings
 * @return  0 on success, -1 otherwise
 *
 * -u default user-id
 * -g default gid
 * -m default mod bits
 * mount path (default arg)
 * device path
 */
int process_args(int argc, char *argv[])
{
  int c;
  
  config.uid = 0;
  config.gid = 0;
  config.mode = 0700;
  config.read_only = false;
  
  if (argc <= 1) {
    return -1;
  }
    
  while ((c = getopt(argc, argv, "u:g:m:r")) != -1) {
    switch (c) {
      case 'u':
        config.uid = atoi(optarg);
        break;

      case 'g':
        config.gid = atoi(optarg);
        break;

      case 'm':
        config.mode = atoi(optarg);
        break;

      case 'r':
        config.read_only = true;
        break;
      
      default:
        break;
    }
  }

  if (optind + 1 >= argc) {
    return -1;
  }

  config.mount_path = argv[optind];
  config.device_path = argv[optind + 1];
  return 0;
}



#ifndef devfs_H
#define devfs_H

//#define NDEBUG

#include <sys/types.h>
#include <stdint.h>


/* Constants
 */
#define NMSG_BACKLOG 			8
#define DEVFS_MAX_INODE 	128
#define DIRENTS_BUF_SZ 		4096


/*
 *
 */
#define ALIGN_UP(val, alignment)                                \
    ((((val) + (alignment) - 1)/(alignment))*(alignment))
#define ALIGN_DOWN(val, alignment)                              \
            ((val) - ((val) % (alignment)))

/*
 *
 */
struct DevfsNode {
  char name[32];
  int32_t inode_nr;
  int32_t parent_inode_nr;
  mode_t mode;
  int32_t uid;
  int32_t gid;
  uint32_t file_offset;
  uint32_t file_size;
} __attribute__((packed));



/*
 * Driver Configuration settings
 */
struct Config
{
  uid_t uid;
  gid_t gid;
  mode_t mode;
  dev_t dev;
};


// prototypes
void init (int argc, char *argv[]);
int process_args(int argc, char *argv[]);
int init_devfs(void);
int mount_device(void);


#endif


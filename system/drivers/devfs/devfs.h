#ifndef devfs_H
#define devfs_H

#define NDEBUG

#include <sys/types.h>
#include <stdint.h>

// types


// Constants

#define MAX_INODE 128

#define DIRENTS_BUF_SZ 4096

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





struct Config {
  void *devfs_image;
  size_t devfs_image_size;
};


// globals
extern int fd;

// prototypes
void init (int argc, char *argv[]);
int processArgs(int argc, char *argv[]);
int mountDevice(void);








#endif


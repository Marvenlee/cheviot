#include "lists.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

struct IFSHeader {
  char magic[4];
  uint32_t node_table_offset;
  int32_t node_cnt;
  uint32_t ifs_size;
} __attribute__((packed));

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

struct IFSNodeContainer {
  LIST_ENTRY(IFSNodeContainer)
  link;
  struct IFSNode node;
  char real_path[1024];
};

LIST_TYPE(IFSNodeContainer) ifsnode_list_t;

// Variables
int output_fd;
ifsnode_list_t ifs_file_list;
size_t ifs_base; // offset of IFS inside kernel.img.
size_t ifs_size; // size of ifs portion of image
int32_t current_inode_nr;

// Prototypes
void WriteBootloader(char *bootloader);
void WriteIFS(char *path_prefix);
void Recursive(DIR *dir, struct IFSNodeContainer *parent);
void WriteFileTable(void);
int WriteBootloaderHeader(void);
int GetNodeCnt(void);
size_t CalculateFileOffsets(size_t offset);
int WriteFileContents(void);

// Macros
#define ALIGN_UP(val, alignment)                                               \
  ((((val) + (alignment)-1) / (alignment)) * (alignment))

#define ALIGN_DOWN(val, alignment) ((val) - ((val) % (alignment)))

/**
 *
 */
int main(int argc, char *argv[]) {
  if (argc != 4) {
    printf("Usage: mkifs output_file bootloader_file ifs_dir\n");
    return -1;
  }

  printf("output     : %s\n", argv[1]);
  printf("bootloader : %s\n", argv[2]);
  printf("root dir   : %s\n", argv[3]);
  output_fd = open(argv[1], O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);

  WriteBootloader(argv[2]);
  WriteIFS(argv[3]);

  close(output_fd);
}

/**
 *
 */
void WriteBootloader(char *bootloader) {
  int fd;
  struct stat stat;
  void *buf;

  printf("WriteBootloader()\n");

  fd = open(bootloader, O_RDONLY);

  if (fd == -1) {
    exit(EXIT_FAILURE);
  }

  fstat(fd, &stat);

  printf("bootloader size = %zu\n", stat.st_size);

  buf = malloc(stat.st_size);

  if (read(fd, buf, stat.st_size) != stat.st_size) {
    exit(EXIT_FAILURE);
  }

  if (write(output_fd, buf, stat.st_size) != stat.st_size) {
    exit(EXIT_FAILURE);
  }

  free(buf);
  close(fd);

  ifs_base = ALIGN_UP(stat.st_size, 4096);
}

/**
 *
 */
void WriteIFS(char *path_prefix) {
  struct IFSNodeContainer *node;
  DIR *root_dir;

  current_inode_nr = 0;

  root_dir = opendir(path_prefix);
  node = malloc(sizeof *node);


  // Do we need this when Recursive() will create "." ?   (But it points here)
  strcpy(node->real_path, path_prefix);
  strcpy(node->node.name, "/");       // Replace with "." or "\0" ????
  node->node.permissions = __S_IFDIR; // stat it correctly
  node->node.inode_nr = current_inode_nr++;
  node->node.parent_inode_nr = 0;
  LIST_ADD_HEAD(&ifs_file_list, node, link);

  Recursive(root_dir, node);
  closedir(root_dir);

  WriteFileTable();    // Calculate file offset when writing.
  WriteFileContents(); // Read each file table entry poth, open file, write it
  // to image
  WriteBootloaderHeader();
}

/**
 *
 */
void Recursive(DIR *dir, struct IFSNodeContainer *parent) {
  struct IFSNodeContainer *node;
  struct dirent *dirent;
  struct stat st;

  printf("Recursive(parent=%s) ...\n", parent->real_path);

  
  // Create a node for "."
  
  node = malloc(sizeof *node);

  strcpy(node->node.name, ".");  
  strncpy(node->real_path, parent->real_path, sizeof node->real_path - 1);
  strncat(node->real_path, "/.", sizeof node->real_path - 1);

  // stat directory to get bits
  if (stat(node->real_path, &st) != 0) {
    printf("Failed to stat %s\n", node->real_path);
    exit(EXIT_FAILURE);
  }

  node->node.permissions = st.st_mode;  
  node->node.inode_nr = parent->node.inode_nr;
  node->node.parent_inode_nr = parent->node.inode_nr;
  node->node.file_size = 0;
  node->node.file_offset = 0x000cafe;

  LIST_ADD_TAIL(&ifs_file_list, node, link);

  current_inode_nr++;

  // Create a node for ".."

  if (parent->node.inode_nr != 0) 
  {
    node = malloc(sizeof *node);
    
    strcpy(node->node.name, "..");
    strncpy(node->real_path, parent->real_path, sizeof node->real_path - 1);
    strncat(node->real_path, "/..", sizeof node->real_path - 1);

    if (stat(node->real_path, &st) != 0) {
      printf("Failed to stat %s\n", node->real_path);
      exit(EXIT_FAILURE);
    }

    node->node.permissions = st.st_mode;  
    node->node.inode_nr = parent->node.parent_inode_nr;
    node->node.parent_inode_nr = parent->node.inode_nr;  
    node->node.file_size = 0;
    node->node.file_offset = 0x000cafe;

    LIST_ADD_TAIL(&ifs_file_list, node, link);

    current_inode_nr++;
  }
    
  // Scan through directory
  
  while ((dirent = readdir(dir)) != NULL) {
    if ((strcmp(dirent->d_name, ".") == 0) ||
        (strcmp(dirent->d_name, "..") == 0)) {
      continue;
    }

    node = malloc(sizeof *node);

    if (node == NULL) {
      exit(EXIT_FAILURE);
    }

    // Relative IFS path stored in node structure
    // strncpy(node->node.path, path, sizeof node->node.path);

    strncpy(node->node.name, dirent->d_name, sizeof node->node.name - 1);

    // filesystem path
    strncpy(node->real_path, parent->real_path, sizeof node->real_path - 1);
    strncat(node->real_path, "/", sizeof node->real_path - 1);
    strncat(node->real_path, node->node.name, sizeof node->real_path - 1);

    if (stat(node->real_path, &st) != 0) {
      printf("Failed to stat %s\n", node->real_path);
    }

    node->node.permissions = st.st_mode;
    
    printf ("node->node.permissions = %0o", node->node.permissions);
    
    node->node.inode_nr = current_inode_nr++;
    node->node.parent_inode_nr = parent->node.inode_nr;

    printf("node->real_path = %s\n", node->real_path);
    printf("node->node.name  = %s\n", node->node.name);

    if (S_ISDIR(st.st_mode)) {
      DIR *new_dir;
      printf("is a dir\n\n");
      node->node.file_size = 0;
      node->node.file_offset = 0x000cafe;
      LIST_ADD_TAIL(&ifs_file_list, node, link);

      new_dir = opendir(node->real_path);

      if (new_dir == NULL) {
        printf("failed to open dir %s\n", node->real_path);
        exit(EXIT_FAILURE);
      }

      Recursive(new_dir, node);
      closedir(new_dir);
    } else if (S_ISREG(st.st_mode)) {
      node->node.file_size = st.st_size;
      node->node.file_offset = 0x000cafe;
      LIST_ADD_TAIL(&ifs_file_list, node, link);
      printf("is a file, sz = %zu\n\n", st.st_size);
    }
  }

  printf("... Recursive done\n");
}

/**
 *
 */
void WriteFileTable(void) {
  struct IFSHeader ifs_header;
  struct IFSNodeContainer *node;
  struct IFSNode free_node;
  int node_cnt;
  int free_node_cnt;
  ssize_t sz;
  size_t node_table_offset;
  size_t file_table_offset;

  printf("WriteFileTable\n");

  node_cnt = GetNodeCnt();
  free_node_cnt = 16;
  node_table_offset = ALIGN_UP(sizeof ifs_header, 128);
  file_table_offset = ALIGN_UP((node_table_offset + ((node_cnt + free_node_cnt) * sizeof node->node)), 4096);

  printf("node_cnt = %d\n, free_node_cnt = %d", node_cnt, free_node_cnt);
  printf("node_table_offset = %zu\n", node_table_offset);
  printf("file_table_offset = %zu\n", file_table_offset);

  ifs_header.magic[0] = 'M';
  ifs_header.magic[1] = 'A';
  ifs_header.magic[2] = 'G';
  ifs_header.magic[3] = 'C';
  ifs_header.node_cnt = node_cnt + free_node_cnt;
  ifs_header.node_table_offset = node_table_offset;
  ifs_header.ifs_size = 0xcafefee1;

  printf("ifs_base = %zu\n", ifs_base);

  printf("ifs_header size = %zu\n", sizeof ifs_header);

  lseek(output_fd, ifs_base, SEEK_SET);
  sz = write(output_fd, &ifs_header, sizeof ifs_header);

  if (sz != sizeof ifs_header) {
    exit(EXIT_FAILURE);
  }

  ifs_size = CalculateFileOffsets(file_table_offset);

  lseek(output_fd, ifs_base + node_table_offset, SEEK_SET);

  node = LIST_HEAD(&ifs_file_list);
  while (node != NULL) {
    sz = write(output_fd, &node->node, sizeof node->node);

    if (sz != sizeof node->node) {
      exit(EXIT_FAILURE);
    }

    node = LIST_NEXT(node, link);
  }

  // Write out slots for free nodes

  for (int t = 0; t < free_node_cnt; t++) {
    free_node.name[0] = '\0';
    free_node.inode_nr = t;
    free_node.parent_inode_nr = -1;
    free_node.permissions = 0;
    free_node.uid = 0;
    free_node.gid = 0;
    free_node.file_offset = 0;
    free_node.file_size = 0;
    sz = write(output_fd, &free_node, sizeof free_node);

    if (sz != sizeof node->node) {
      exit(EXIT_FAILURE);
    }
  }
}

/**
 *
 */
int WriteBootloaderHeader(void) {
  uint32_t magic_header_orig[2];
  uint32_t magic_header[2];
  ssize_t sz;

  printf("WriteBootloaderHeader\n");

  magic_header[0] = ifs_base;
  magic_header[1] =
      ifs_size; // Can be calculated by adding all the file sizes and offsets
  // Or calculating seek position at end

  lseek(output_fd, 16, SEEK_SET);
  sz = read(output_fd, &magic_header_orig, sizeof magic_header_orig);

  printf("read magic header = %08x, %08x\n", magic_header_orig[0],
         magic_header_orig[1]);

  lseek(output_fd, 16, SEEK_SET);
  sz = write(output_fd, &magic_header, sizeof magic_header);

  lseek(output_fd, 16, SEEK_SET);
  sz = read(output_fd, &magic_header_orig, sizeof magic_header_orig);

  printf("read magic header = %08x, %08x\n", magic_header_orig[0],
         magic_header_orig[1]);

  if (sz != sizeof magic_header) {
    exit(EXIT_FAILURE);
  }

  printf("ifs_base = %zu\n", ifs_base);
  printf("ifs_size = %zu\n", ifs_size);

  return 0;
}

/**
 *
 */
int GetNodeCnt(void) {
  struct IFSNodeContainer *node;
  int cnt = 0;

  node = LIST_HEAD(&ifs_file_list);
  while (node != NULL) {
    cnt++;
    node = LIST_NEXT(node, link);
  }

  return cnt;
}

/**
 *
 */
size_t CalculateFileOffsets(size_t offset) {
  struct IFSNodeContainer *node;
  size_t file_size = offset;

  node = LIST_HEAD(&ifs_file_list);
  while (node != NULL) {
    node->node.file_offset = offset;
    offset = ALIGN_UP(node->node.file_offset + node->node.file_size, 4096);
    file_size = node->node.file_offset + node->node.file_size;
    node = LIST_NEXT(node, link);
  }

  return file_size;
}

/**
 *
 */
int WriteFileContents(void) {
  struct IFSNodeContainer *node;
  int fd;
  void *buf;
  ssize_t sz;

  node = LIST_HEAD(&ifs_file_list);
  while (node != NULL) {
    if (S_ISREG(node->node.permissions)) {
      lseek(output_fd, ifs_base + node->node.file_offset, SEEK_SET);

      fd = open(node->real_path, O_RDONLY);
      buf = malloc(node->node.file_size);
      sz = read(fd, buf, node->node.file_size);

      if (sz != node->node.file_size) {
        exit(EXIT_FAILURE);
      }

      sz = write(output_fd, buf, node->node.file_size);

      if (sz != node->node.file_size) {
        exit(EXIT_FAILURE);
      }

      free(buf);
      close(fd);
    } else {
      printf("skipping dir %s\n", node->node.name);
    }
    node = LIST_NEXT(node, link);
  }

  return 0;
}

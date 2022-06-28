#include "globals.h"
#include "sys/debug.h"
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/syscalls.h>
#include <poll.h>
#include <unistd.h>


/*
 *
 */
int process_args(int argc, char *argv[]);
int map_io_registers(void);
int mount_device(void);
void sdread(int ino, struct fsreq *req);
void sdwrite(int ino, struct fsreq *req);

/*
 *
 */
int main(int argc, char *argv[]) {
  struct fsreq req;
  int sc;
  int pid;
  struct pollfd pfd;
  
  KLog(">>>>>>>>>>>> Hello from SDCard driver <<<<<<<<<<<<<<");

  sc = process_args(argc, argv);
  if (sc != 0) {
    KLog("processArgs FAILED, sc = %d", sc);
    exit(-1);
  }

  sc = map_io_registers();
  if (sc != 0) {
    KLog("mapIORegisters FAILED, sc = %d", sc);
    exit(-1);
  }

  KLog ("Calling sd_card_init");
  sc = sd_card_init(&bdev);
  if (sc != 0) {
    KLog("sd_card_init FAILED, sc = %d", sc);
    exit(-1);
  }
  
  fd = mount_device();
  if (fd < 0) {
    KLog("**** failed to makenode /dev/sd1, exiting sdcard.exe");
    exit(-1);
  }

  buf = virtualalloc(NULL, 4096, PROT_READ | PROT_WRITE | CACHE_UNCACHEABLE);

  
  KLog("SD: Main Event loop...");
  while (1) {
//    KLog ("SD: Polling");

    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    sc = poll (&pfd, 1, -1);
//    KLog ("SD: Polling awakened");
    
    if (pfd.revents & POLLIN) {
//      KLog ("SD:  Received POLLIN");
      while ((sc = receivemsg(fd, &pid, &req, sizeof req)) == sizeof req) {
        switch (req.cmd) {
          case CMD_READ:
            sdread(pid, &req);
            break;

          case CMD_WRITE:
            sdwrite(pid, &req);
            break;

          default:
            exit(-1);
        }
      }
      
      if (sc != 0) {
        KLog("sd: receivemsg err = %d", sc, strerror(-sc));
        exit(-1);
      }
    }
  }

  exit(0);
}

/*
 *
 */
int process_args(int argc, char *argv[]) {
  int c;

  // -u default user-id
  // -g default gid
  // -m default mod bits
  // -D debug level ?
  // mount path (default arg)

  KLog ("processArgs");

  if (argc <= 1) {
    KLog ("SDC : processArgs argc = %d", argc);
    exit(-1);
  }


  while ((c = getopt(argc, argv, "u:g:m:")) != -1) {
    switch (c) {
    case 'u':
      config.uid = atoi(optarg);
//      KLog ("uid = %d", config.uid);
      break;

    case 'g':
      config.gid = atoi(optarg);
//      KLog ("gid = %d", config.gid);
      break;

    case 'm':
      config.mode = atoi(optarg);
//      KLog ("mode = %d", config.mode);
      break;
    }
  }

  if (optind >= argc) {
    KLog ("SDC : processArgs failed, optind = %d, argc = %d", optind, argc);
    exit(-1);
  }

  strncpy(config.pathname, argv[optind], sizeof config.pathname);
  return 0;
}

/*
 *
 */
int map_io_registers(void) {
  KLog ("mapIORegisters");
  
  emmc_base = (uint32_t)virtualallocphys(0, 8092, PROT_READ | PROT_WRITE  | CACHE_UNCACHEABLE,
                                         (void *)EMMC_BASE_PA);
  mb_addr = (uint32_t)virtualallocphys(0, 8092, PROT_READ | PROT_WRITE  | CACHE_UNCACHEABLE,
                                       (void *)MB_ADDR_PA);
  timer_clo = (uint32_t)virtualallocphys(0, 8092, PROT_READ | PROT_WRITE | CACHE_UNCACHEABLE,
                                         (void *)TIMER_BASE_PA);
  timer_clo += TIMER_CLO_OFFSET;
  mbox_base = (uint32_t)virtualallocphys(0, 8092, PROT_READ | PROT_WRITE | CACHE_UNCACHEABLE,
                                         (void *)MBOX_BASE_PA);
  mbox_base += MBOX_BASE_OFFSET;

  return 0;
}

/*
 *
 */
int mount_device(void) {
  int fd;
  struct stat stat;

  stat.st_dev = 0; // Get from config, or returned by Mount() (sb index?)
  stat.st_ino = 0;
  stat.st_mode = 0777 | _IFBLK; // default to read/write of device-driver uid/gid.
  stat.st_uid = 0;   // default device driver uid
  stat.st_gid = 0;   // default gid
  stat.st_blksize = bdev->block_size;

  // TODO :Calculate device size
  stat.st_size = 0x0000ffffffff0000;
  stat.st_blocks = stat.st_size / stat.st_blksize;

  fd = mount("/dev/sd1", 0, &stat);
  return fd;
}

/*
 *
 */
void sdread(int pid, struct fsreq *req) {
  struct fsreply reply;
  off64_t offset;
  size_t count;
  ssize_t nbytes_read;
  size_t xfered;
  int sc;

  reply.cmd = CMD_READ;

  offset = req->args.read.offset;

  //	if (offset >= bdev->block_size * bdev->num_blocks)
  //		count = 0;
  //	else if (req->args.read.sz < ((bdev->block_size * bdev->num_blocks) -
  // offset))
  count = req->args.read.sz;
  //	else
  //		count = (bdev->block_size * bdev->num_blocks) - offset;

  KLog("SDCard SDRead pid = %d, offs = %d sz = %d", pid, (int)offset, count);

  seekmsg(fd, pid, sizeof *req + sizeof reply);
  
  xfered = 0;
  
  while (xfered < count)
  {
      sc = sd_read(bdev, buf, 512, (off_t)(offset / 512));      
      nbytes_read = (count < 512) ? count : 512;
      xfered += nbytes_read;
      offset += nbytes_read;

      writemsg(fd, pid, buf, nbytes_read);
      
      if (nbytes_read != 512) {
        break;
      }
  }
  
  seekmsg(fd, pid, sizeof *req);
  reply.args.read.nbytes_read = xfered;  
  writemsg(fd, pid, &reply, sizeof reply);
  replymsg(fd, pid, 0);
}

/*
 *
 */
void sdwrite(int pid, struct fsreq *req) {
  struct fsreply reply;
  off64_t offset;
  size_t count;
  int sc;

  reply.cmd = CMD_WRITE;

  offset = req->args.write.offset;

  //	if (offset >= bdev->block_size * bdev->num_blocks)
  //		count = 0;
  //	else if (req->args.read.sz < ((bdev->block_size * bdev->num_blocks) -
  // offset))
  count = req->args.write.sz;
  //	else
  //		count = (bdev->block_size * bdev->num_blocks) - offset;
  seekmsg(fd, pid, sizeof *req + sizeof reply);
  readmsg(fd, pid, buf, 512);
  KLog ("**** NOT WRITING TO SD CARD, NOT ENABLED *****");
  
//  sc = sd_write(bdev, buf, 512, (off_t)(offset / 512));
  reply.args.write.nbytes_written = 512;
  
  seekmsg(fd, pid, sizeof *req + sizeof reply);
  writemsg(fd, pid, &reply, sizeof reply);
  replymsg(fd, pid, 0);
}


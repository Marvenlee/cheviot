
#define LOG_LEVEL_INFO

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
#include <sys/event.h>
#include "sdcard.h"
#include "globals.h"
#include <sys/rpi_mailbox.h>
#include <sys/rpi_gpio.h>
#include <time.h>
#include <sys/time.h>

/* @brief   The SDCard block device driver
 *
 * @param   argc, argument count passed on command line
 * @param   argv, arguments passed on command line
 * @return  0 on success, non-zero on failure
 *
 * The SDCard driver mounts the whole block device and also
 * individual detected partitions as separate mount points.
 */
int main(int argc, char *argv[])
{
  struct fsreq req;
  int sc;
  struct kevent ev;
  int portid;
  msgid_t msgid;
  struct bdev_unit *unit;
   
  log_info("sdcard starting");
   
  init(argc, argv);  

  while (1) {
    kevent(kq, NULL, 0, &ev, 1, NULL);
		
    portid = ev.ident;
    unit = ev.udata;
    
    if (ev.filter == EVFILT_MSGPORT) {
      while ((sc = getmsg(portid, &msgid, &req, sizeof req)) == sizeof req) {
        switch (req.cmd) {
          case CMD_READ:
            sdcard_read(unit, msgid, &req);
            break;

          case CMD_WRITE:
            sdcard_write(unit, msgid, &req);
            break;

          default:
            log_warn("sdcard: unknown command: %d", req.cmd);
            replymsg(portid, msgid, -ENOTSUP, NULL, 0);
            break;
        }
      }
      
      if (sc != 0) {
        log_error("sdcard: getmsg sc=%d %s", sc, strerror(errno));
        exit(EXIT_FAILURE);
      }
    }
  }

  exit(0);
}


/* @brief   Handle the CMD_READ message to read a block
 *
 * @param   unit, parameters and state of the whole device or a partition
 * @param   msgid, message id returned by receivemsg
 * @param   req, filesystem request message header
 *
 * This assumes blocks are 512 bytes in size 
 * TODO: Check for block alignment of offset and size
 * TODO: Check within range of unit
 */
void sdcard_read(struct bdev_unit *unit, msgid_t msgid, struct fsreq *req)
{
  off64_t block_no;
  off64_t offset;
  size_t remaining;
  off_t chunk_start;
  size_t chunk_size;  
  size_t left;
  size_t xfered;
  int sc;

  xfered = 0;
  offset = req->args.read.offset;
  remaining = req->args.read.sz;  
	  
  while (remaining > 0) {
      block_no = ((off64_t)unit->start + (offset / 512));
      chunk_start = offset % 512;
      left = 512 - chunk_start;

      sc = sd_read(bdev, buf, 512, block_no);      

      chunk_size = (left < remaining) ? left : remaining;
      
      writemsg(unit->portid, msgid, buf+chunk_start, chunk_size, xfered);

      xfered += chunk_size;
      offset += chunk_size;
      remaining -= chunk_size;
  }


  replymsg(unit->portid, msgid, xfered, NULL, 0);
}


/* @brief   Handle the CMD_WRITE message to write a block 
 *
 * @param   unit, parameters and state of the whole device or a partition
 * @param   msgid, message id returned by receivemsg
 * @param   req, filesystem request message header
 *
 * TODO: Check for block alignment of offset and size
 * TODO: Check within range of unit
 */
void sdcard_write(struct bdev_unit *unit, msgid_t msgid, struct fsreq *req)
{
  off64_t block_no;
  off64_t offset;
  size_t remaining;
  off_t chunk_start;
  size_t chunk_size;  
  size_t left;
  size_t xfered;
  int sc;

  xfered = 0;
  offset = req->args.write.offset;
  remaining = req->args.write.sz;  
  
  while (remaining > 0) {
      block_no = ((off64_t)unit->start + (offset / 512));
      chunk_start = offset % 512;
      left = 512 - chunk_start;

      chunk_size = (left < remaining) ? left : remaining;
      
      if (chunk_size != 512) {
          sc = sd_read(bdev, buf, 512, block_no);      
      }
      
      readmsg(unit->portid, msgid, buf+chunk_start, chunk_size, sizeof(struct fsreq) + xfered);
      sc = sd_write(bdev, buf, 512, block_no);

      xfered += chunk_size;
      offset += chunk_size;
      remaining -= chunk_size;
  }

  replymsg(unit->portid, msgid, xfered, NULL, 0);
}


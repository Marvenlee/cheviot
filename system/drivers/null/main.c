/*
 * Copyright 2019  Marven Gilhespie
 *
 * Licensed under the Apache License, segment_id 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscalls.h>
#include <unistd.h>
#include <sys/fsreq.h>
#include <sys/debug.h>
#include <sys/event.h>


/* @brief   The "/dev/null" device driver
 */
void main(int argc, char *argv[])
{
  int sc;
  int portid;
  int kq;
  msgid_t msgid;
  struct fsreq req;
  int nevents;
  struct kevent ev;
  struct stat stat;

  stat.st_dev = 0; // FIXME: Use a known major/minor number for /dev/null
  stat.st_ino = 0;
  stat.st_mode = 0777 | S_IFCHR;
  stat.st_uid = 0;
  stat.st_gid = 0;
  stat.st_blksize = 0;
  stat.st_size = 0;
  stat.st_blocks = 0;
  
  portid = mount("/dev/null", 0, &stat);
  if (portid < 0) {
    exit(EXIT_FAILURE);
  }

  kq = kqueue();
  if (kq < 0) {
    exit(EXIT_FAILURE);
  }
  
  EV_SET(&ev, portid, EVFILT_MSGPORT, EV_ADD | EV_ENABLE, 0, 0, 0); 
  kevent(kq, &ev, 1,  NULL, 0, NULL);

  while (1) {
    errno = 0;
    nevents = kevent(kq, NULL, 0, &ev, 1, NULL);

    if (nevents == 1 && ev.ident == portid && ev.filter == EVFILT_MSGPORT) {
      while ((sc = getmsg(portid, &msgid, &req, sizeof req)) == sizeof req) {
        switch (req.cmd) {
          case CMD_READ:
            replymsg(portid, msgid, 0, NULL, 0);
            break;

          case CMD_WRITE:
            replymsg(portid, msgid, 0, NULL, 0);
            break;

          case CMD_ISATTY:
            replymsg(portid, msgid, -ENOTTY, NULL, 0);
            break;

          default:
            replymsg(portid, msgid, -ENOSYS, NULL, 0);
            break;
        }
      }      
      
      if (sc != 0) {
        exit(EXIT_FAILURE);
      }
    }
  }
  
  exit(0);
}


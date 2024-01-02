/*
 * Copyright 2023  Marven Gilhespie
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

#define LOG_LEVEL_INFO

#include "ext2.h"
#include "globals.h"


/* @brief   Ext filesystem handler main loop
 *
 */
int main(int argc, char *argv[])
{
  struct kevent ev;
  struct fsreq req;
  int sc;
  int nevents;
  
  log_info("ext2fs started");
  
  init(argc, argv);
  
  EV_SET(&ev, portid, EVFILT_MSGPORT, EV_ADD | EV_ENABLE, 0, 0, 0); 
  kevent(kq, &ev, 1, NULL, 0, NULL);

  while (1) {
    nevents = kevent(kq, NULL, 0, &ev, 1, NULL);
  
    if (nevents == 1 && ev.ident == portid && ev.filter == EVFILT_MSGPORT) {
      while ((sc = getmsg(portid, &msgid, &req, sizeof req)) == sizeof req) {      
        switch (req.cmd) {
          case CMD_READ:
            ext2_read(&req);
            break;

          case CMD_WRITE:
            ext2_write(&req);
            break;

          case CMD_LOOKUP:
            ext2_lookup(&req);
            break;
          
          case CMD_CLOSE:
            ext2_close(&req);
            break;
          
          case CMD_CREATE:
            ext2_create(&req);

          case CMD_READDIR:
            ext2_readdir(&req);
            break;

          case CMD_UNLINK:
            ext2_unlink(&req);
            break;

          case CMD_RMDIR:
            ext2_rmdir(&req);
            break;

          case CMD_MKDIR:
            ext2_mkdir(&req);
            break;

          case CMD_MKNOD:
            ext2_mknod(&req);
            break;

          case CMD_RENAME:
            ext2_rename(&req);
            break;

          case CMD_CHMOD:
            ext2_chmod(&req);
            break;

          case CMD_CHOWN:
            ext2_chown(&req);
            break;

          // TODO: Add VNODEATTR

          default:
            exit(EXIT_FAILURE);
        }
      }

      if (sc != 0) {
        log_error("ext2fs: getmsg err = %d, %s", sc, strerror(-sc));
        exit(-1);
      }
    }
  }

  exit(0);
}







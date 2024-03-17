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

#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/syscalls.h>
#include <sys/fsreq.h>
#include <sys/debug.h>
#include <sys/lists.h>
#include <sys/event.h>
#include <task.h>
#include "trng.h"
#include "globals.h"
#include "random.h"


/* @brief   Main function of the /dev/random driver
 *
 */
void main(int argc, char *argv[])
{
  struct fsreq req;
  int sc;
  int nevents;
  msgid_t msgid;
  struct timespec timeout;

  log_info("random: main");
   
  init(argc, argv);

  timeout.tv_sec = 0;
  timeout.tv_nsec = 500000000;
    
  EV_SET(&setev, portid, EVFILT_MSGPORT, EV_ADD | EV_ENABLE, 0, 0, 0); 
  // EV_SET(&setev[1], interrupt_fd, EVFILT_IRQ, EV_ADD | EV_ENABLE, 0, 0, 0); 
  kevent(kq, &setev, 1,  NULL, 0, NULL);

  while (1) {
    errno = 0;
    nevents = kevent(kq, NULL, 0, &ev, 1, &timeout);

    if (nevents == 1 && ev.ident == portid && ev.filter == EVFILT_MSGPORT) {
      while ((sc = getmsg(portid, &msgid, &req, sizeof req)) == sizeof req) {
        switch (req.cmd) {
          case CMD_READ:
            cmd_read(msgid, &req);
            break;

          case CMD_WRITE:
            cmd_write(msgid, &req);
            break;

          default:
            log_warn("random: unknown command: %d", req.cmd);
            replymsg(portid, msgid, -ENOTSUP, NULL, 0);
            break;
        }
      }      
      
      if (sc != 0) {
        log_error("random: getmsg sc=%d %s", sc, strerror(errno));
        exit(EXIT_FAILURE);
      }
    }
  }

  exit(0);
}


/*
 *
 */
void cmd_read(msgid_t msgid, struct fsreq *req)
{
	size_t nbytes;
	int words;
	
 	nbytes = (sizeof random_buf < req->args.read.sz) ? sizeof random_buf : req->args.read.sz;

	words = ALIGN_UP(nbytes, sizeof (uint32_t)) / sizeof (uint32_t);

	if (words > sizeof random_buf / sizeof (uint32_t)) {
		words = sizeof random_buf / sizeof (uint32_t);
	}

	for (int t=0; t<words; t++) {
		trng_data_read(&random_buf);
	}      	
      	
  replymsg(portid, msgid, nbytes, random_buf, nbytes);
}


/*
 * 
 */
void cmd_write(msgid_t msgid, struct fsreq *req)
{
  replymsg(portid, msgid, -EPERM, NULL, 0);
}



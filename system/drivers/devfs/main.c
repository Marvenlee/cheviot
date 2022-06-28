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

#include "devfs.h"
#include "globals.h"
#include "sys/debug.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fsreq.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/syscalls.h>
#include <unistd.h>
#include <poll.h>


static void devfsLookup(int ino, struct fsreq *req);
static void devfsClose(int ino, struct fsreq *req);
static void devfsReaddir(int ino, struct fsreq *req);
static void devfsMknod(int ino, struct fsreq *req);
static void devfsUnlink(int ino, struct fsreq *req);



/*
 *
 */

int main(int argc, char *argv[]) {
  struct fsreq req;
  int sc;
  int pid;
  struct pollfd pfd;
  
  KLog ("--------- devfs ---------");
  
  init(argc, argv);

  KLog ("----- devfs starting main loop");

  while (1) {  
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    KLog ("******** ----- devfs poll");
    sc = poll (&pfd, 1, -1);
    KLog ("******** ----- devfs poll returned sc = %d", sc);
      
    if (pfd.revents & POLLIN) {
      while ((sc = receivemsg(fd, &pid, &req, sizeof req)) == sizeof req) {
        switch (req.cmd) {
          case CMD_LOOKUP:
            devfsLookup(pid, &req);
            break;
          
          case CMD_CLOSE:
            devfsClose(pid, &req);
            break;
          
          case CMD_READDIR:
            devfsReaddir(pid, &req);
            break;

          case CMD_MKNOD:
            devfsMknod(pid, &req);
            break;

          case CMD_UNLINK:
            devfsUnlink(pid, &req);
            break;

          default:
            replymsg(fd, pid, -ENOSYS);
        }
      }
      
      if (sc != 0) {
        KLog("devfs: receivemsg err = %d, %s", sc, strerror(-sc));
        // TODO: Close FD, remount a start again.
        exit(-1);
      }
    }
  }

  exit(0);
}

/**
 *
 */
static void devfsLookup(int pid, struct fsreq *req) {
  struct DevfsNode *devfs_dir_node;
  struct DevfsNode *node;
  struct fsreply reply;
  char name[256];

  readmsg(fd, pid, name, req->args.lookup.name_sz);

  name[60] = '\0';

  KLog ("devfs - devfsLookup, name = %s", name);
  
  if (req->args.lookup.dir_inode_nr < 0 || req->args.lookup.dir_inode_nr >= MAX_INODE) {
    goto exit;
  }
  
  devfs_dir_node = &devfs_inode_table[req->args.lookup.dir_inode_nr];

  for (int inode_nr = 1; inode_nr < MAX_INODE; inode_nr++) {
//    if (devfs_inode_table[inode_nr].parent_inode_nr != 0) {      
//        Sleep(1);
//      continue;
//    }

//    KLog ("devfs ino %d, name = %s", inode_nr, devfs_inode_table[inode_nr].name);

    if (strcmp(devfs_inode_table[inode_nr].name, name) == 0) {
      node = &devfs_inode_table[inode_nr];

//      KLog ("devfs: node = %08x", (uint32_t)node);
//      KLog ("devfs: node->inode_nr = %d", node->inode_nr);
      
      reply.args.lookup.status = 0;
      reply.args.lookup.inode_nr = node->inode_nr;
      reply.args.lookup.size = node->file_size;
      reply.args.lookup.uid = 0;
      reply.args.lookup.gid = 0;
      reply.args.lookup.mode = S_IRWXU | S_IRWXG | S_IRWXO | (devfs_inode_table[inode_nr].mode & _IFMT);
      
      KLog ("dewfs lookup mode = %o, reply: %o", devfs_inode_table[inode_nr].mode, reply.args.lookup.mode);


      writemsg(fd, pid, &reply, sizeof reply);
      replymsg(fd, pid, 0);
//      KLog ("devfsLookup done ok !!!!!!!!!!!!!");
      return;
    }
  }

exit:
  
  reply.args.lookup.status = -1;
  writemsg(fd, pid, &reply, sizeof reply);
  replymsg(fd, pid, -1);
  KLog ("devfsLookup done err !!!!!!!!!!!");
}


/*
 *
 */
static void devfsClose(int pid, struct fsreq *req) {
  struct fsreply reply;

  reply.cmd = CMD_CLOSE;
  reply.args.close.status = 0;
  writemsg(fd, pid, &reply, sizeof reply);
  replymsg(fd, pid, 0);

}


/*
 *
 */
static void devfsReaddir(int pid, struct fsreq *req) {
  struct DevfsNode *node;
  struct dirent *dirent;
  int len;
  int dirent_buf_sz;
  struct fsreply reply;
  int cookie;
  int sc;
        
  cookie = req->args.readdir.offset;
  dirent_buf_sz = 0;

  if (cookie == 0) {
    cookie = 1;
  }

  while (dirent_buf_sz < DIRENTS_BUF_SZ - 64) {
    if (cookie >= MAX_INODE) {
      break;
    }
    
    node = &devfs_inode_table[cookie];

    if (node->name[0] != '\0') {
      dirent = (struct dirent *)(dirents_buf + dirent_buf_sz);
      len = strlen(node->name);
      strncpy(dirent->d_name, node->name, len + 1);
      dirent->d_reclen = ALIGN_UP(
          (uint8_t *)&dirent->d_name[len + 1] - (uint8_t *)dirent, 16);

      dirent->d_ino = node->inode_nr;
      dirent->d_cookie = cookie;
      
      dirent_buf_sz += dirent->d_reclen;
    }

    cookie++;
  }


  reply.args.readdir.offset = cookie;
  reply.args.readdir.nbytes_read = dirent_buf_sz;

  sc = seekmsg(fd, pid, sizeof *req + sizeof reply);
  
  if (sc != 0)
  {
    KLog ("********* devfsReaddir seekmsg != 0, sc = %d", sc);
    // TODO: Panic
  }

  sc = writemsg(fd, pid, &dirents_buf[0], dirent_buf_sz);

  if (sc != dirent_buf_sz)
  {
        // TODO: Panic
    KLog("****** devfsReaddir writemsg actual = %d", sc);
  } 
  

  seekmsg(fd, pid, sizeof *req);
  writemsg(fd, pid, &reply, sizeof reply);
  replymsg(fd, pid, 0);
}


/*
 *
 */
static void devfsMknod(int pid, struct fsreq *req) {
  struct DevfsNode *dir_node;
  struct DevfsNode *node;
  struct fsreply reply;
  char name[256];
  bool exists = false;

  
  readmsg(fd, pid, name, req->args.mknod.name_sz);
  
  KLog ("***** devfs - Mknod name = %s", name);
  
  dir_node = &devfs_inode_table[req->args.mknod.dir_inode_nr];

  for (int inode_nr = 1; inode_nr < MAX_INODE; inode_nr++) {
    if (strcmp(devfs_inode_table[inode_nr].name, name) == 0) {
      exists = true;
      break;
    }
  }
  
  if (exists == true) {  
    KLog ("devfs - mknod - exists");
    reply.args.mknod.status = -EEXIST;
    writemsg(fd, pid, &reply, sizeof reply);
    replymsg(fd, pid, -EEXIST);
    return;
  }
  
  node = NULL;
  
  for (int inode_nr = 1; inode_nr < MAX_INODE; inode_nr++) {
    if (devfs_inode_table[inode_nr].name[0] == '\0') {
      node = &devfs_inode_table[inode_nr];
      break;
    }
  }

  if (node == NULL) {
    KLog ("devfs - mknod - enomem");
    reply.args.mknod.status = -ENOMEM;
    writemsg(fd, pid, &reply, sizeof reply);
    replymsg(fd, pid, -EEXIST);
    return;
  }
   
  strcpy(node->name, name);
  node->mode = req->args.mknod.mode;
  node->uid = req->args.mknod.uid;
  node->gid = req->args.mknod.gid;
  
  reply.args.mknod.status = 0;
  reply.args.mknod.inode_nr = node->inode_nr;
  reply.args.mknod.mode = node->mode;
  reply.args.mknod.uid = node->uid;
  reply.args.mknod.gid = node->gid;
  reply.args.mknod.size = 0;
  
  writemsg(fd, pid, &reply, sizeof reply);
  replymsg(fd, pid, 0);

  KLog ("devfs - Mknod success, mode = %o", node->mode);
}

/*
 *
 */
static void devfsUnlink(int ino, struct fsreq *req) {
}


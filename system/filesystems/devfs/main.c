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
#include <sys/event.h>
#include "devfs.h"
#include "globals.h"
#include "sys/debug.h"
#include <sys/event.h>

static void devfsLookup(msgid_t msgid_t, struct fsreq *req);
static void devfsClose(msgid_t msgid_t, struct fsreq *req);
static void devfsReaddir(msgid_t msgid_t, struct fsreq *req);
static void devfsMknod(msgid_t msgid_t, struct fsreq *req);
static void devfsUnlink(msgid_t msgid_t, struct fsreq *req);


/*
 *
 */
int main(int argc, char *argv[])
{
  struct fsreq req;
  int sc;
  int nevents;
  struct kevent ev;
  msgid_t msgid;
  
  init(argc, argv);

  EV_SET(&ev, portid, EVFILT_MSGPORT, EV_ADD | EV_ENABLE, 0, 0, 0); 
  kevent(kq, &ev, 1,  NULL, 0, NULL);

  while (1) {
    errno = 0;
    nevents = kevent(kq, NULL, 0, &ev, 1, NULL);

    if (nevents == 1 && ev.ident == portid && ev.filter == EVFILT_MSGPORT) {
      while ((sc = getmsg(portid, &msgid, &req, sizeof req)) == sizeof req) {
        switch (req.cmd) {
          case CMD_LOOKUP:
            devfsLookup(msgid, &req);
            break;
          
          case CMD_CLOSE:
            devfsClose(msgid, &req);
            break;
          
          case CMD_READDIR:
            devfsReaddir(msgid, &req);
            break;

          case CMD_MKNOD:
            devfsMknod(msgid, &req);
            break;

          case CMD_UNLINK:
            devfsUnlink(msgid, &req);
            break;

          default:
            log_error("sdcard unknown command: %d", req.cmd);
            exit(EXIT_FAILURE);
        }
      }
      
      if (sc != 0) {
        log_error("devfs: getmsg sc=%d %s", sc, strerror(errno));
        exit(EXIT_FAILURE);
      }
    }
  }

  exit(0);
}

/**
 *
 */
static void devfsLookup(msgid_t msgid, struct fsreq *req)
{
  struct fsreply reply;
  struct DevfsNode *devfs_dir_node;
  struct DevfsNode *node;
  char name[256];

  memset (&reply, 0, sizeof reply);

  readmsg(portid, msgid, name, req->args.lookup.name_sz, sizeof *req);
  name[60] = '\0';

  if (req->args.lookup.dir_inode_nr < 0 || req->args.lookup.dir_inode_nr >= DEVFS_MAX_INODE) {
    replymsg(portid, msgid, -EINVAL, NULL, 0);
    return;
  }
  
  devfs_dir_node = &devfs_inode_table[req->args.lookup.dir_inode_nr];

  for (int inode_nr = 1; inode_nr < DEVFS_MAX_INODE; inode_nr++) {
    if (strcmp(devfs_inode_table[inode_nr].name, name) == 0) {
      node = &devfs_inode_table[inode_nr];

      reply.args.lookup.inode_nr = node->inode_nr;
      reply.args.lookup.size = node->file_size;
      reply.args.lookup.mode = S_IRWXU | S_IRWXG | S_IRWXO
                               | (S_IFMT & devfs_inode_table[inode_nr].mode);      
      reply.args.lookup.uid = 0;
      reply.args.lookup.gid = 0;

      // TODO: Add date fields

      replymsg(portid, msgid, 0, &reply, sizeof reply);
      return;
    }
  }

  replymsg(portid, msgid, -ENOENT, NULL, 0);
}


/*
 *
 */
static void devfsClose(msgid_t msgid, struct fsreq *req)
{
  replymsg(portid, msgid, 0, NULL, 0);
}


/*
 *
 */
static void devfsReaddir(msgid_t msgid, struct fsreq *req)
{
  struct fsreply reply;
  struct DevfsNode *node;
  struct dirent *dirent;
  int len;
  int reclen;
  int dirent_buf_sz;
  int cookie;
  int sc;
  int max_reply_sz;

  memset (&reply, 0, sizeof reply);
          
  cookie = req->args.readdir.offset;
  dirent_buf_sz = 0;

  max_reply_sz = (req->args.readdir.sz < DIRENTS_BUF_SZ) ? req->args.readdir.sz : DIRENTS_BUF_SZ;
  
  if (cookie == 0) {
    cookie = 1;
  }

  while (1) {
    if (cookie >= DEVFS_MAX_INODE) {
      break;
    }
    
    node = &devfs_inode_table[cookie];

    if (node->name[0] != '\0') {
      dirent = (struct dirent *)(dirents_buf + dirent_buf_sz);
      len = strlen(node->name);
     
      reclen = ALIGN_UP(
          ((intptr_t)&dirent->d_name[len] + 1 - (intptr_t)dirent), 8);
  
      if (dirent_buf_sz + reclen <= max_reply_sz) {       
        memset(dirents_buf + dirent_buf_sz, 0, reclen);
        strcpy(&dirent->d_name[0], node->name);      
        dirent->d_ino = node->inode_nr;
        dirent->d_cookie = cookie;
        dirent->d_reclen = reclen;

        dirent_buf_sz += reclen;
      } else {
        // Retry on next readdir
        break;
      }
    }
    
    cookie++;
  }

  
  writemsg(portid, msgid, &dirents_buf[0], dirent_buf_sz, sizeof reply);

  reply.args.readdir.offset = cookie;
  replymsg(portid, msgid, dirent_buf_sz, &reply, sizeof reply);
}


/*
 *
 */
static void devfsMknod(msgid_t msgid, struct fsreq *req)
{
  struct fsreply reply;
  struct DevfsNode *dir_node;
  struct DevfsNode *node;
  char name[256];
  bool exists = false;

  memset (&reply, 0, sizeof reply);
  
  readmsg(portid, msgid, name, req->args.mknod.name_sz, sizeof *req);
  
  dir_node = &devfs_inode_table[req->args.mknod.dir_inode_nr];

  for (int inode_nr = 1; inode_nr < DEVFS_MAX_INODE; inode_nr++) {
    if (strcmp(devfs_inode_table[inode_nr].name, name) == 0) {
      exists = true;
      break;
    }
  }
  
  if (exists == true) {  
    replymsg(portid, msgid, -EEXIST, NULL, 0);
    return;
  }
  
  node = NULL;
  
  for (int inode_nr = 1; inode_nr < DEVFS_MAX_INODE; inode_nr++) {
    if (devfs_inode_table[inode_nr].name[0] == '\0') {
      node = &devfs_inode_table[inode_nr];
      break;
    }
  }

  if (node == NULL) {
    replymsg(portid, msgid, -ENOSPC, NULL, 0);
    return;
  }
   
  strcpy(node->name, name);
  node->mode = req->args.mknod.mode;
  node->uid = req->args.mknod.uid;
  node->gid = req->args.mknod.gid;
  
  reply.args.mknod.inode_nr = node->inode_nr;
  reply.args.mknod.mode = node->mode;
  reply.args.mknod.uid = node->uid;
  reply.args.mknod.gid = node->gid;
  reply.args.mknod.size = 0;
  
  replymsg(portid, msgid, 0, &reply, sizeof reply);
}

/*
 *
 */
static void devfsUnlink(msgid_t msgid, struct fsreq *req)
{
  replymsg(portid, msgid, -ENOTSUP, NULL, 0);
}


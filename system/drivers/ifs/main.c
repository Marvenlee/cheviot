/*
 * Copyright 2014  Marven Gilhespie
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

/*
 * Initial File System driver. Implements a read-only file system that is merged
 * with the kernel image.
 */
 
#include "ifs.h"
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


static void ifsLookup(int ino, struct fsreq *req);
static void ifsClose(int ino, struct fsreq *req);
static void ifsRead(int ino, struct fsreq *req);
static void ifsReaddir(int ino, struct fsreq *req);


/*
 *
 */
int main(int argc, char *argv[]) {
  struct fsreq req;
  int sc;
  int pid;
  struct pollfd pfd;
  
  init(argc, argv);

  while (1) {  
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    sc = poll (&pfd, 1, -1);
      
    if (pfd.revents & POLLIN) {
      while ((sc = ReceiveMsg(fd, &pid, &req, sizeof req)) == sizeof req) {
        switch (req.cmd) {
          case CMD_LOOKUP:
            ifsLookup(pid, &req);
            break;
          
          case CMD_CLOSE:
            ifsClose(pid, &req);
            break;
          
          case CMD_READ:
            ifsRead(pid, &req);
            break;

          case CMD_READDIR:
            ifsReaddir(pid, &req);
            break;

          default:
            ReplyMsg(fd, pid, -ENOSYS);
        }
      }
      
      if (sc != 0) {
        KLog("ifs: ReceiveMsg err = %d, %s", sc, strerror(-sc));
        exit(-1);
      }
    }
  }

  exit(0);
}

/**
 *
 */
static void ifsLookup(int pid, struct fsreq *req) {
  struct IFSNode *ifs_dir_node;
  struct IFSNode *node;
  struct fsreply reply;
  char name[256];

  ReadMsg(fd, pid, name, req->args.lookup.name_sz);

  name[255] = '\0';
  
  ifs_dir_node = &ifs_inode_table[req->args.lookup.dir_inode_nr];

  for (int inode_nr = 0; inode_nr < ifs_header->node_cnt; inode_nr++) {
    if (ifs_inode_table[inode_nr].parent_inode_nr != ifs_dir_node->inode_nr) {
      continue;
    }

    if (strcmp(ifs_inode_table[inode_nr].name, name) == 0) {
      node = &ifs_inode_table[inode_nr];
      
      reply.args.lookup.status = 0;
      reply.args.lookup.inode_nr = node->inode_nr;
      reply.args.lookup.size = node->file_size;
      reply.args.lookup.uid = 0;
      reply.args.lookup.gid = 0;
      reply.args.lookup.mode = S_IRWXU | S_IRWXG | S_IRWXO | (ifs_inode_table[inode_nr].permissions & _IFMT);

      WriteMsg(fd, pid, &reply, sizeof reply);
      ReplyMsg(fd, pid, 0);
      return;
    }
  }
  
  reply.args.lookup.status = -1;
  WriteMsg(fd, pid, &reply, sizeof reply);
  ReplyMsg(fd, pid, -1);
}

/*
 *
 */
static void ifsClose(int pid, struct fsreq *req) {
  struct fsreply reply;

  reply.cmd = CMD_CLOSE;
  reply.args.close.status = 0;
  WriteMsg(fd, pid, &reply, sizeof reply);
  ReplyMsg(fd, pid, 0);
}

/*
 *
 */
static void ifsRead(int pid, struct fsreq *req){

  struct IFSNode *rnode;
  size_t nbytes_read;
  size_t remaining;
  uint8_t *src;
  struct fsreply reply;
  off64_t offset;
  size_t count;
  
  offset = req->args.read.offset;
  count = req->args.read.sz;
  rnode = &ifs_inode_table[req->args.read.inode_nr];

  SeekMsg(fd, pid, sizeof *req + sizeof reply);

  src = (uint8_t *)ifs_header + rnode->file_offset + offset;  
  remaining = rnode->file_size - offset;
  nbytes_read = (count < remaining) ? count : remaining;
  
  if (nbytes_read > 1) {
    nbytes_read = WriteMsg(fd, pid, src, nbytes_read);    
  }
  
  SeekMsg(fd, pid, sizeof *req);
  reply.args.read.nbytes_read = nbytes_read;
  WriteMsg(fd, pid, &reply, sizeof reply);
  ReplyMsg(fd, pid, 0);
}

/*
 *
 */
static void ifsReaddir(int pid, struct fsreq *req) {
  struct IFSNode *node;
  struct dirent *dirent;
  int len;
  int dirent_buf_sz;
  struct fsreply reply;
  int cookie;
  int sc;
        
  cookie = req->args.readdir.offset;
  dirent_buf_sz = 0;


  while (dirent_buf_sz < DIRENTS_BUF_SZ - 64) {
    if (cookie >= ifs_header->node_cnt) {
      break;
    }
    
    node = &ifs_inode_table[cookie];

    if (node->name[0] != '\0' && node->parent_inode_nr == req->args.readdir.inode_nr) {
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

  sc = SeekMsg(fd, pid, sizeof *req + sizeof reply);
  
  if (sc != 0)
  {
    exit(-1);
  }

  sc = WriteMsg(fd, pid, &dirents_buf[0], dirent_buf_sz);

  if (sc != dirent_buf_sz)
  {
    exit(-1);
  } 
  
  SeekMsg(fd, pid, sizeof *req);
  WriteMsg(fd, pid, &reply, sizeof reply);
  ReplyMsg(fd, pid, 0);
}




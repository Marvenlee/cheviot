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
 * Initial File System driver. Implements a read-only file system for bootstrapping
 * the OS. This is the root process. It forks to create a process that mounts
 * and handles the IFS file system. The root process itself execs /sbin/root which
 * assumes the role of the real root process. 
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

static void exec_init(void);
static void reap_processes(void);
static void ifs_message_loop(void);
static void ifs_lookup(int ino, struct fsreq *req);
static void ifs_close(int ino, struct fsreq *req);
static void ifs_read(int ino, struct fsreq *req);
static void ifs_readdir(int ino, struct fsreq *req);


/* @Brief main function of Image File System driver and root process
 *
 * Kernel starts the IFS.exe task, proc(0) root process
 * IFS.exe is passed base address and size of IFS
 * IFS.exe process forks. IFS image now mapped into 2 processes, 
 * both read-only (no COW) unless perms change.
 *
 * Forked process proc(1) becomes IFS file system handler and creates root mount "/".
 *
 * proc(0), IFS.exe unmaps IFS image
 * proc(0), waits for proc(1) to mount it's FS.
 *
 * <Optionally>
 * proc(0), IFS.exe does an Exec to become root.exe, no longer ifs.exe and only 1 process has IFS image mapped.
 * Root.exe forks and starts init.exe, proc(2)
 * </Optionally>
 *
 */
int main(int argc, char *argv[]) {
  int rc;

  KLog("ifs.exe main hello\n");
  KLog ("ifs.exe main, argc = %d", argc);

  if (argc < 3) {
    return EXIT_FAILURE;
  }

  ifs_image = (void *)strtoul(argv[1], NULL, 16);
  ifs_image_size = strtoul(argv[2], NULL, 10);

  KLog ("ifs image = %08x", (uint32_t)ifs_image);
  KLog ("ifs image size = %u", ifs_image_size);

  KLog ("calling init_ifs\n");
  
  if (init_ifs() != 0) {
    return EXIT_FAILURE;
  }
  
  if (mount_device() != 0) {
    return EXIT_FAILURE;
  }
  
  KLog ("forking...");
  
  rc = fork();

  if (rc > 0) {
    KLog ("parent of fork");
    // virtualfree(ifs_image, ifs_image_size);
    close(fd);
    fd = -1;
    exec_init();    
  } else if (rc == 0) {
    KLog ("child of fork");
    ifs_message_loop();
  } else {
    KLog ("fork failed");
    return EXIT_FAILURE;
  }
  
  return 0;
}

/*
 *
 */
static void exec_init(void) {
  int rc;
  
  KLog ("exec_init, forking again");
  
  rc = fork();
  if (rc > 0) {
    KLog ("parent of fork 2, root reaper");
    reap_processes();
  } else if (rc == 0) {  
    KLog ("child of fork 2, exec sbin/init");
    sleep(1);   // Wait for root to be mounted, ideally wait on '/' to change, get a notification    
    rc = execl("/sbin/init", NULL);
        
    KLog ("exec failed, %d", rc);
  }
  
  if (rc == -1) {
    KLog ("fork failed");
    // Kill IFS sub process  
    exit(EXIT_FAILURE);  
  }
}

/*
 *
 */
static void reap_processes(void)
{
  KLog ("reap processes");
  
  while(waitpid(-1, NULL, 0) != 0)
  {
    sleep(1);
  }
}

/*
 *
 */
static void ifs_message_loop(void)
{  
  struct fsreq req;
  struct pollfd pfd;
  int pid;
  int rc;
  
  KLog ("ifs_message loop");
  
  while (1) {  
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;

    rc = poll (&pfd, 1, -1);
      
    if (pfd.revents & POLLIN) {
      while ((rc = receivemsg(fd, &pid, &req, sizeof req)) == sizeof req) {
        switch (req.cmd) {
          case CMD_LOOKUP:
            ifs_lookup(pid, &req);
            break;
          
          case CMD_CLOSE:
            ifs_close(pid, &req);
            break;
          
          case CMD_READ:
            ifs_read(pid, &req);
            break;

          case CMD_READDIR:
            ifs_readdir(pid, &req);
            break;

          default:
            replymsg(fd, pid, -ENOSYS);
        }
      }
      
      if (rc != 0) {
        KLog("ifs: ReceiveMsg err = %d, %s", sc, strerror(-sc));
        exit(-1);
      }
    }
  }

  exit(0);
}

/*
 *
 */
static void ifs_lookup(int pid, struct fsreq *req) {
  struct IFSNode *ifs_dir_node;
  struct IFSNode *node;
  struct fsreply reply;
  char name[256];

  readmsg(fd, pid, name, req->args.lookup.name_sz);

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

      writemsg(fd, pid, &reply, sizeof reply);
      replymsg(fd, pid, 0);
      return;
    }
  }
  
  reply.args.lookup.status = -1;
  writemsg(fd, pid, &reply, sizeof reply);
  replymsg(fd, pid, -1);
}

/*
 *
 */
static void ifs_close(int pid, struct fsreq *req) {
  struct fsreply reply;

  reply.cmd = CMD_CLOSE;
  reply.args.close.status = 0;
  writemsg(fd, pid, &reply, sizeof reply);
  replymsg(fd, pid, 0);
}

/*
 *
 */
static void ifs_read(int pid, struct fsreq *req) {
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

  seekmsg(fd, pid, sizeof *req + sizeof reply);

  src = (uint8_t *)ifs_header + rnode->file_offset + offset;  
  remaining = rnode->file_size - offset;
  nbytes_read = (count < remaining) ? count : remaining;
  
  if (nbytes_read > 1) {
    nbytes_read = writemsg(fd, pid, src, nbytes_read);    
  }
  
  seekmsg(fd, pid, sizeof *req);
  reply.args.read.nbytes_read = nbytes_read;
  writemsg(fd, pid, &reply, sizeof reply);
  replymsg(fd, pid, 0);
}

/*
 *
 */
static void ifs_readdir(int pid, struct fsreq *req) {
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

  if (seekmsg(fd, pid, sizeof *req + sizeof reply) != 0) {
    exit(-1);
  }

  if (writemsg(fd, pid, &dirents_buf[0], dirent_buf_sz) != dirent_buf_sz) {
    exit(-1);
  } 
  
  seekmsg(fd, pid, sizeof *req);
  writemsg(fd, pid, &reply, sizeof reply);
  replymsg(fd, pid, 0);
}




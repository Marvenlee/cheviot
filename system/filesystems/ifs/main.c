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
#include <sys/wait.h>
#include <unistd.h>
#include <sys/event.h>


// Static prototypes
static void exec_init(void);
static void reap_processes(void);
static void ifs_message_loop(void);
static void ifs_lookup(msgid_t msgid, struct fsreq *req);
static void ifs_close(msgid_t msgid, struct fsreq *req);
static void ifs_read(msgid_t msgid, struct fsreq *req);
static void ifs_write(msgid_t msgid, struct fsreq *req);
static void ifs_readdir(msgid_t msgid, struct fsreq *req);


/* @Brief   Main function of Initial File System (IFS) driver and root process
 *
 * Kernel starts the IFS.exe task, proc(0) root process
 * IFS.exe is passed base address and size of IFS
 * IFS.exe process forks. IFS image now mapped into 2 processes, 
 * both read-only (no COW) unless perms change.
 *
 * Child process proc(1) becomes IFS file system handler and creates root mount "/".
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
int main(int argc, char *argv[])
{
  int rc;
  
  init_ifs(argc, argv);
    
  rc = fork();

  if (rc > 0) {
    // We are still the root process (pid 0 ?)
    close(portid);
    portid = -1;
    exec_init();
  } else if (rc == 0) {
    // We are the second process (pid 1 ?), the IFS handler process
    ifs_message_loop();
  } else {
    log_error("ifs fork failed, exiting: rc:%d", rc);
    return EXIT_FAILURE;
  }
  
  return 0;
}


/* @brief Fork a process to act as 
 * 
 */
static void exec_init(void)
{
  int rc;

  log_info("exec_init()");
    
  rc = fork();

  log_info ("forked, rc = %d", rc);

  if (rc > 0) {
    // We are still the root process (pid 0), the root reaper process
    reap_processes();
  } else if (rc == 0) {  
    log_info("sleeping before execing /sbin/init");

//    sleep(5);   // FIXME: This shouldn't need to wait as mount created prior to forking.

    // We are the third process (pid 2), the init process.
    log_info("execing /sbin/init");
    
    rc = execl("/sbin/init", NULL);        
    log_error("ifs exec failed, %d", rc);
    exit(EXIT_FAILURE);  
  }
  
  if (rc == -1) {
    log_error("ifs second fork failed");
    exit(EXIT_FAILURE);  
  }
}


/* @brief   Act as root process, reap any dead zombie processes
 * 
 */
static void reap_processes(void)
{
  while(waitpid(-1, NULL, 0) != 0) {
    sleep(5);
  }
}


/* @brief   IFS file system handler
 *
 */
static void ifs_message_loop(void)
{  
  struct fsreq req;
  int rc;
  int nevents;
  struct kevent ev;
  msgid_t msgid;
    
  EV_SET(&ev, portid, EVFILT_MSGPORT, EV_ADD | EV_ENABLE, 0, 0, 0); 
  kevent(kq, &ev, 1,  NULL, 0, NULL);

  while (1) {
    errno = 0;

    nevents = kevent(kq, NULL, 0, &ev, 1, NULL);

    if (nevents == 1 && ev.ident == portid && ev.filter == EVFILT_MSGPORT) {
      while ((rc = getmsg(portid, &msgid, &req, sizeof req)) == sizeof req) {
        switch (req.cmd) {
          case CMD_LOOKUP:
            ifs_lookup(msgid, &req);
            break;
          
          case CMD_CLOSE:
            ifs_close(msgid, &req);
            break;
          
          case CMD_READ:
            ifs_read(msgid, &req);
            break;

          case CMD_WRITE:
            ifs_write(msgid, &req);
            break;

          case CMD_READDIR:
            ifs_readdir(msgid, &req);
            break;

          default:
            log_warn("ifs: Unknown message cmd:%d\n", req.cmd);
            replymsg(portid, msgid, -ENOTSUP, NULL, 0);
            break;
        }
      }
      
      if (rc != 0) {
        log_error("ifs exiting: getmsg err = %d, %s", rc, strerror(errno));
        exit(-1);
      }
    }
  }
}


/* @brief   Handle VFS Lookup message
 *
 */
static void ifs_lookup(msgid_t msgid, struct fsreq *req)
{
  struct IFSNode *ifs_dir_node;
  struct IFSNode *node;
  struct fsreply reply;
  char name[256];
  ssize_t sz;
   
  memset(&reply, 0, sizeof reply);
  
  sz = readmsg(portid, msgid, name, req->args.lookup.name_sz, sizeof *req);

  name[255] = '\0';
  ifs_dir_node = &ifs_inode_table[req->args.lookup.dir_inode_nr];

  for (int inode_nr = 0; inode_nr < ifs_header->node_cnt; inode_nr++) {
    if (ifs_inode_table[inode_nr].parent_inode_nr != ifs_dir_node->inode_nr) {
      continue;
    }

    if (strcmp(ifs_inode_table[inode_nr].name, name) == 0) {
      node = &ifs_inode_table[inode_nr];
      
      reply.args.lookup.inode_nr = node->inode_nr;
      reply.args.lookup.size = node->file_size;
      reply.args.lookup.mode = S_IRWXU | S_IRWXG | S_IRWXO | (S_IFMT & node->permissions);
      reply.args.lookup.uid = 0;
      reply.args.lookup.gid = 0;      
      // FIXME: Add date fields
      
      replymsg(portid, msgid, 0, &reply, sizeof reply);
      return;
    }
  }
  
  replymsg(portid, msgid, -ENOENT, &reply, sizeof reply);
}


/* @brief   Handle VFS close message
 *
 */
static void ifs_close(msgid_t msgid, struct fsreq *req)
{
  replymsg(portid, msgid, 0, NULL, 0);
}


/* @brief   Handle VFS strategy message for reading files
 *
 */
static void ifs_read(msgid_t msgid, struct fsreq *req)
{
  struct IFSNode *rnode;
  ssize_t nbytes_read;
  size_t remaining;
  uint8_t *src;
  off64_t offset;
  size_t count;

  rnode = &ifs_inode_table[req->args.read.inode_nr];  
  offset = req->args.read.offset;
  count = req->args.read.sz;  
  
  src = (uint8_t *)ifs_header + rnode->file_offset + offset;  
  remaining = rnode->file_size - offset;
  nbytes_read = (count < remaining) ? count : remaining;
  
  if (nbytes_read >= 1) {
    nbytes_read = writemsg(portid, msgid, src, nbytes_read, 0);
  }

  replymsg(portid, msgid, nbytes_read, NULL, 0);  
}


/* @brief   Handle VFS strategy message for reading files
 *
 */
static void ifs_write(msgid_t msgid, struct fsreq *req)
{
  log_warn("CMD_STRATEGY write not allowed on IFS");
  replymsg(portid, msgid, -EPERM, NULL, 0);  
}


/* @brief   Handle VFS readdir message to read a directory
 *
 */
static void ifs_readdir(msgid_t msgid, struct fsreq *req)
{
  struct fsreply reply;
  struct IFSNode *node;
  struct dirent *dirent;
  int len;
  int reclen;
  int dirent_buf_sz;
  int cookie;
  int sc;
  int max_reply_sz;
  
  memset(&reply, 0, sizeof reply);
  
  cookie = req->args.readdir.offset;
  dirent_buf_sz = 0;
  
  max_reply_sz = (req->args.readdir.sz < DIRENTS_BUF_SZ) ? req->args.readdir.sz : DIRENTS_BUF_SZ;
  
  while (1) {
    if (cookie < 0 || cookie >= ifs_header->node_cnt) {
      break;
    }
    
    node = &ifs_inode_table[cookie];

    if (node->name[0] != '\0' && node->parent_inode_nr == req->args.readdir.inode_nr) {
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



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

#include "fat.h"
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
#include <sys/event.h>

/*
 *
 */

int main(int argc, char *argv[])
{
  struct fsreq req;
  int sc;
  struct kevent ev;
  msgid_t msgid;
  
  init(argc, argv);
  
  KLog("FAT: Main Event loop...");
  
  EV_SET(&ev, portid, EVFILT_MSGPORT, EV_ADD | EV_ENABLE,0, 0, 0); 
  kevent(kq, &ev, 1,  NULL, 0, NULL);

  while (1) {
    kevent(kq, NULL, 0, &ev, 1, NULL);
  
    if (ev.ident == portid && ev.filter == EVFILT_MSGPORT) {
      while ((sc = getmsg(portid, &msgid, &req, sizeof req)) == sizeof req) {

        switch (req.cmd) {
          case CMD_LOOKUP:
            fatLookup(msgid, &req);
            break;
          
          case CMD_CLOSE:
            fatClose(msgid, &req);
            break;
          
          case CMD_CREATE:
            fatCreate(msgid, &req);

          case CMD_READ:
            fatRead(msgid, &req);
            break;

          case CMD_WRITE:
            fatWrite(msgid, &req);
            break;
            
            // TODO: CMD_STRATEGY

          case CMD_READDIR:
            fatReadDir(msgid, &req);
            break;

          case CMD_UNLINK:
            fatUnlink(msgid, &req);
            break;

          case CMD_RMDIR:
            fatRmDir(msgid, &req);
            break;

          case CMD_MKDIR:
            fatMkDir(msgid, &req);
            break;

          case CMD_MKNOD:
            fatMkNod(msgid, &req);
            break;

          case CMD_RENAME:
            fatRename(msgid, &req);
            break;

          // TODO: Add VNODEATTR

          default:
            KLog ("unknown fat cmd");
            exit(-1);
        }
      }

      if (sc != 0) {
        KLog("fat: getmsg err = %d, %s", sc, strerror(errno));
        exit(-1);
      }
    }
  }

  exit(0);
}

/*
 */

void fatLookup(msgid_t msgid, struct fsreq *req)
{
  struct fsreply reply;
  char name[256];
  int32_t rc;
  struct FatNode *dirnode;
  struct FatNode *node;
  int sz;
  
  readmsg(portid, msgid, name, req->args.lookup.name_sz, sizeof *req);

  KLog ("fatLookup, name = %s", name);

  dirnode = FindNode(req->args.lookup.dir_inode_nr);

  if (dirnode == NULL) {
    KLog ("fatLookup : failed to find dirnode");
    replymsg(portid, msgid, -1, &reply, sizeof reply);
    return;
  }

  rc = lookup(dirnode, name, &node);
  
  if (rc == 0) {
    reply.args.lookup.inode_nr = node->inode_nr;
    reply.args.lookup.size = node->dirent.size;
    reply.args.lookup.uid = config.uid;
    reply.args.lookup.gid = config.gid;
    reply.args.lookup.mode = S_IRWXU | S_IRWXG | S_IRWXO;

    if (node->dirent.attributes & ATTR_DIRECTORY) {
      reply.args.lookup.mode |= _IFDIR;
    }

    replymsg(portid, msgid, 0, &reply, sizeof reply);    
    return;
  }

  replymsg(portid, msgid, -ENOENT, NULL, 0);
}


/*
 *
 */
void fatClose(msgid_t msgid, struct fsreq *req)
{
  // Remove from cache of inodes

  replymsg(portid, msgid, 0, NULL, 0);
}


/*
 *
 */
void fatCreate(msgid_t msgid, struct fsreq *req)
{
  replymsg(portid, msgid, -ENOTSUP, NULL, 0);
}


/*
 *
 */
void fatRead(msgid_t msgid, struct fsreq *req)
{
  ssize_t nbytes_read;
  struct FatNode *node;
  off_t offset;
  size_t count;

  node = FindNode(req->args.read.inode_nr);

  if (node == NULL) {
    replymsg(portid, msgid, -EINVAL, NULL, 0);
    return;
  }

  offset = req->args.read.offset;
  count = req->args.read.sz;
  
  if (count > sizeof read_buf) {
    count = sizeof read_buf;
  }
  
  nbytes_read = readFile(node, read_buf, count, offset);

  if (nbytes_read > 1) {
    nbytes_read = writemsg(portid, msgid, read_buf, nbytes_read, 0);
  }  
}


/*
 *
 */
void fatWrite(msgid_t msgid, struct fsreq *req)
{
  struct FatNode *node;
  ssize_t nbytes_written;
  off_t offset;
  size_t count;

  node = FindNode(req->args.read.inode_nr);

  if (node == NULL) {
    replymsg(portid, msgid, -EINVAL, NULL, 0);
    return;
  }

  offset = req->args.write.offset;
  count = req->args.write.sz;
  
  readmsg(portid, msgid, write_buf, count, sizeof *req);
  
  nbytes_written = writeFile(node, write_buf, count, offset);

  replymsg(portid, msgid, nbytes_written, NULL, 0);
}


/*
 *
 */
void fatReadDir(msgid_t msgid, struct fsreq *req)
{
  struct fsreply reply;
  struct FatNode *node;
  struct FatDirEntry fdirent;
  struct dirent *dirent;
  int len;
  int rc;
  off_t cookie;
  int dirent_buf_sz = 0;
  int dirent_read_cnt = 0;
  int reclen;
  int aligned_reclen;

  node = FindNode(req->args.readdir.inode_nr);
  cookie = req->args.readdir.offset;

/* Should not do a readdir on non dir
  if (!(node->dirent.attributes & ATTR_DIRECTORY)) {
    reply.args.readdir.nbytes_read = -ENOTDIR;
    replymsg(fd, -ENOTDIR, &reply, sizeof reply);
  }
*/

  while (dirent_buf_sz < DIRENTS_BUF_SZ - 64) {
    rc = fat_dir_read(node, &fdirent, cookie, NULL, NULL);

    if (rc == 0) {
      if (fdirent.name[0] == DIRENTRY_FREE) {
      } else if (fdirent.name[0] != DIRENTRY_DELETED &&
                 !(fdirent.attributes & ATTR_VOLUME_ID)) {
        dirent = (struct dirent *)(dirents_buf + dirent_buf_sz);

        len = FatDirEntryToASCIIZ(dirent->d_name, &fdirent);

        reclen = (uint8_t *)&dirent->d_name[len + 1] - (uint8_t *)dirent;
        aligned_reclen = ALIGN_UP(reclen, 16);
        dirent->d_reclen = aligned_reclen;
        dirent->d_ino =
            (fdirent.first_cluster_hi << 16) | fdirent.first_cluster_lo;
        dirent->d_cookie = cookie;

        memset(dirents_buf + dirent_buf_sz + reclen, 0,
               aligned_reclen - reclen);
        dirent_buf_sz += dirent->d_reclen;
        dirent_read_cnt++;
      }

      cookie++;
    } else {
      // TODO: Can we set  reply.args.offset = -1 to indicate EOF ?
      break;
    }
  }

  writemsg(portid, msgid, dirents_buf, reply.args.readdir.nbytes_read, sizeof reply);

  reply.args.readdir.offset = cookie;

  replymsg(portid, msgid, dirent_buf_sz, &reply, sizeof reply);

  KLog("FatReaddir DONE");
}


/*
 *
 */
void fatMkNod(msgid_t msgid, struct fsreq *req)
{
  replymsg(portid, msgid, -ENOTSUP, NULL, 0);
}


/*
 *
 */
void fatRename(msgid_t msgid, struct fsreq *req)
{
  replymsg(portid, msgid, -ENOTSUP, NULL, 0);
}


/*
 *
 */
void fatTruncate(msgid_t msgid, struct fsreq *req)
{
  struct FatNode *node;
  int32_t size;
  int status = -EINVAL;

  node = FindNode(req->args.truncate.inode_nr);
  size = req->args.truncate.size;

  if (size == node->dirent.size) {
    status = 0;
  } else if (size < 0) {
    status = -EINVAL;
  } else if (size < node->dirent.size) {
    status = truncateFile(node, size);
  } else {
    status = extendFile(node, size);
  }

  replymsg(portid, msgid, status, NULL, 0);
}


/*
 *
 */
void fatMkDir(msgid_t msgid, struct fsreq *req)
{
  struct FatDirEntry dot, dotdot;
  struct FatDirEntry dirent;
  uint32 sector;
  uint32 sector_offset;
  struct FatNode *node;
  int t;
  int status = -EINVAL;
  struct FatNode *parent;
  char name[256];


  readmsg(portid, msgid, name, req->args.mkdir.name_sz, sizeof *req);

  parent = FindNode(req->args.mkdir.dir_inode_nr);

  if (FatASCIIZToDirEntry(&dirent, name) != 0) {
    replymsg(portid, msgid, -EINVAL, NULL, 0);
    return;
  }

  dirent.attributes = ATTR_DIRECTORY;
  dirent.reserved = 0;
  dirent.size = 0;
  //	FatSetTime (&dirent, ST_CTIME | ST_ATIME | ST_MTIME);
  SetFirstCluster(&dirent, CLUSTER_EOC);

  if (FatCreateDirEntry(parent, &dirent, &sector, &sector_offset) == 1) {
    if ((node = AllocNode(&dirent, sector, sector_offset)) != NULL) {
      dot.attributes = ATTR_DIRECTORY;
      dot.reserved = 0;
      dot.first_cluster_hi = 0; // TODO
      dot.first_cluster_lo = 0;
      dot.size = 0;
      //			FatSetTime (&dot, ST_CTIME | ST_ATIME |
      //ST_MTIME);
      SetFirstCluster(&dot, GetFirstCluster(&node->dirent));

      dot.name[0] = '.';
      for (t = 1; t < 8; t++)
        dot.name[t] = ' ';
      for (t = 0; t < 3; t++)
        dot.extension[t] = ' ';

      dotdot.attributes = ATTR_DIRECTORY;
      dotdot.reserved = 0;
      dotdot.first_cluster_hi = 0; // TODO
      dotdot.first_cluster_lo = 0;
      dotdot.size = 0;
      //			FatSetTime (&dotdot, ST_CTIME | ST_ATIME |
      //ST_MTIME);

      if (parent == &fsb.root_node)
        SetFirstCluster(&dotdot, 0);
      else
        SetFirstCluster(&dotdot, GetFirstCluster(&parent->dirent));

      dotdot.name[0] = '.';
      dotdot.name[1] = '.';
      for (t = 2; t < 8; t++)
        dotdot.name[t] = ' ';
      for (t = 0; t < 3; t++)
        dotdot.extension[t] = ' ';

      if (FatCreateDirEntry(node, &dot, NULL, NULL) == 1 &&
          FatCreateDirEntry(node, &dotdot, NULL, NULL) == 1) {
        //				FatSetTime (&parent->dirent, ST_CTIME |
        //ST_ATIME | ST_MTIME);
        FlushDirent(parent);
        
        replymsg(portid, msgid, status, NULL, 0);
        return;
      }

      FreeClusters(GetFirstCluster(&node->dirent));
      FreeNode(node);
    }

    FatDeleteDirEntry(sector, sector_offset);
  }

  replymsg(portid, msgid, 0, NULL, 0);
}


/*
 *
 */
void fatUnlink(msgid_t msgid, struct fsreq *req)
{
  struct FatNode *parent;
  struct FatNode *node;
  int status = -EINVAL;
  char name[256];
  int rc;
  
  parent = FindNode(req->args.unlink.dir_inode_nr);

  if (parent == NULL) {
    replymsg(portid, msgid, -EINVAL, NULL, 0);
    return;
  }
  
  readmsg(portid, msgid, name, req->args.unlink.name_sz, sizeof *req);

  rc = lookup(parent, name, &node);  

  if (rc != 0) {
    replymsg(portid, msgid, -ENOENT, NULL, 0);
    return;
  }
  
  if ((node->dirent.attributes & ATTR_DIRECTORY) == 0) {
    if (node->reference_cnt == 1) {
      FreeClusters(GetFirstCluster(&node->dirent));
      node->dirent.name[0] = DIRENTRY_DELETED;

      //			FatSetTime (&parent->dirent, ST_CTIME |
      //ST_MTIME);
      FlushDirent(parent);
      FlushDirent(node);
      FreeNode(node);

      replymsg(portid, msgid, 0, NULL, 0);
      return;
    } else {
      status = -EBUSY;
    }
  } else {
    status = -EISDIR;
  }
  
  FreeNode(node);

  replymsg(portid, msgid, status, NULL, 0);
}

/*
 *
 */

void fatRmDir(msgid_t msgid, struct fsreq *req)
{
  struct fsreply reply;
  struct FatNode *parent;
  struct FatNode *node;
  int status = -EINVAL;
  char name[256];
  int rc;
  
  parent = FindNode(req->args.rmdir.dir_inode_nr);
  
  if (parent == NULL) {
    replymsg(portid, msgid, -EINVAL, NULL, 0);
    return;
  }

  readmsg(portid, msgid, name, req->args.rmdir.name_sz, sizeof *req);

  rc = lookup(parent, name, &node);  

  if (rc != 0) {
    replymsg(portid, msgid, -ENOENT, NULL, 0);
    return;
  }

  if (node != &fsb.root_node) {
    if (node->dirent.attributes & ATTR_DIRECTORY) {
      if (node->reference_cnt == 1) {
        if (IsDirEmpty(node) == 0) {
          FreeClusters(GetFirstCluster(&node->dirent));
          node->dirent.name[0] = DIRENTRY_DELETED;

          //					FatSetTime (&parent->dirent, ST_CTIME |
          //ST_MTIME);
          FlushDirent(parent);
          FlushDirent(node);
          FreeNode(node);

          replymsg(portid, msgid, status, NULL, 0);
          return;
        } else
          status = -EEXIST;
      } else
        status = -EBUSY;
    } else
      status = -ENOTDIR;
  } else
    status = -EINVAL;

  FreeNode(node);

  replymsg(portid, msgid, status, NULL, 0);
}


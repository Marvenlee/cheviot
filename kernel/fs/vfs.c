/*
 * Copyright 2014  Marven Gilhespie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
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
 * Functions to create the messages sent to the filesystem and device driver
 * servers.
 */

//#define KDEBUG

#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/vm.h>
#include <kernel/utility.h>
#include <string.h>

/*
 *
 */
int vfs_lookup(struct VNode *dvnode, char *name,
                             struct VNode **result) {
  struct fsreq req;
  struct fsreply reply;
  struct VNode *vnode;
  size_t name_sz;
  struct SuperBlock *sb;
  struct Msg msg;
  struct IOV iov[3];
  
  Info ("name = %08x", (uint32_t)name);
  Info ("vfs_lookup(%s)", name);
  
  memset(&req, 0, sizeof req);
  sb = dvnode->superblock;
  name_sz = StrLen(name) + 1;

  Info ("name_sz = %d", name_sz);

  req.cmd = CMD_LOOKUP;
  req.args.lookup.dir_inode_nr = dvnode->inode_nr;
  req.args.lookup.name_sz = name_sz;

  iov[0].addr = &req;
  iov[0].size = sizeof req;
  iov[1].addr = name;
  iov[1].size = name_sz;
  iov[2].addr = &reply;
  iov[2].size = sizeof reply;

  msg.iov_cnt = 3;
  msg.iov = iov;

  Info ("sb = %08x", sb);
  Info ("sb->msgport = %08x", sb->msgport);
  Info ("KSendMsg lookup");
  
  KSendMsg(&sb->msgport, dvnode, &msg);

  Info ("KSendMsg reply received");

  if (reply.args.lookup.status == 0) {

    // FIXME: Can we move this into higher level? 
    // Is there a potential race condition?  What is it fails to allocate
    // but the message has created a node on disk?
    
    if (reply.args.lookup.inode_nr == dvnode->inode_nr) {
      VNodeIncRef(dvnode);
      vnode = dvnode;
    } else {
      vnode = VNodeGet(dvnode->superblock, reply.args.lookup.inode_nr);
    }

    if (vnode == NULL) {
      vnode = VNodeNew(sb, reply.args.lookup.inode_nr);

      if (vnode == NULL) {
        return -ENOMEM;
      }

      // FIXME: Need to get nlink
      vnode->nlink = 1;      
      // vnode->data = NULL;
      vnode->reference_cnt = 1;
      vnode->size = reply.args.lookup.size;      
      vnode->uid = reply.args.lookup.uid;  
      vnode->gid = reply.args.lookup.gid;
      vnode->mode = reply.args.lookup.mode;
      vnode->flags = V_VALID;

      *result = vnode;
      return 0;
    } else {
      *result = vnode;
      return 0;
    }
  }

  *result = NULL;
  return -EIO;
}

/*
 *
 */
int vfs_create(struct VNode *dir, char *name, int oflags,
                             struct stat *stat, struct VNode **result) {
  return -ENOTSUP;
}

/*
 *
 */
ssize_t vfs_delwrite(struct VNode *vnode, void *dst, size_t nbytes, off64_t *offset) {

  // Queue message on device
  return 0;

}

/*
 * TODO: To be called by ReplyMsg() to update state of delayed_write Buf.
 */ 
ssize_t vfs_delwrite_done (struct VNode *vnode, size_t nbytes, off64_t *offset, int status) {
  struct Buf *buf;
  
  buf = GetBlk(vnode, *offset);
  
  buf->flags &= ~B_BUSY;
  vnode->busy = false;
  TaskWakeupAll(&buf->rendez);
  
  return 0;
}

/*
 *
 */
ssize_t vfs_read(struct VNode *vnode, void *dst, size_t nbytes, off64_t *offset) {
  struct fsreq req;
  struct fsreply reply;
  struct Msg msg;
  struct IOV iov[3];
  int sc;
  struct SuperBlock *sb;

  KASSERT(vnode != NULL);
  KASSERT(dst != NULL);
        
  sb = vnode->superblock;
  
  KASSERT(sb != NULL);
  
  req.cmd = CMD_READ;
  req.args.read.inode_nr = vnode->inode_nr;

  if (offset != NULL) {
    req.args.read.offset = *offset;
  } else {
    req.args.read.offset = 0;
  }

  req.args.read.sz = nbytes;
  
  iov[0].addr = &req;
  iov[0].size = sizeof req;
  iov[1].addr = &reply;
  iov[1].size = sizeof reply;
  iov[2].addr = dst;
  iov[2].size = nbytes;
  msg.iov_cnt = 3;
  msg.iov = iov;

  sc = KSendMsg(&sb->msgport, vnode, &msg);

  if (sc == 0) {
    if (reply.args.read.nbytes_read >= 0) {

      if (offset != NULL) {
        *offset += reply.args.read.nbytes_read;
      }
    }    
  } else {
    goto exit;    
  }

  return reply.args.read.nbytes_read;

exit:
  return sc;
}


/*
 * Move to cache.c
 */
ssize_t vfs_write(struct VNode *vnode, void *src, size_t nbytes, off64_t *offset) {
  struct fsreq req;
  struct fsreply reply;
  struct Msg msg;
  struct IOV iov[3];
  int sc;
  struct SuperBlock *sb;
    
  sb = vnode->superblock;

//  Info ("vfs_write nbytes = %d", nbytes);

  req.cmd = CMD_WRITE;
  req.args.write.inode_nr = vnode->inode_nr;

  if (offset != NULL) {
    req.args.write.offset = *offset;
  } else {
    req.args.write.offset = 0;
  }

  req.args.write.sz = nbytes;

  iov[0].addr = &req;
  iov[0].size = sizeof req;
  iov[1].addr = &reply;
  iov[1].size = sizeof reply;
  iov[2].addr = src;
  iov[2].size = nbytes;

  msg.iov_cnt = 3;
  msg.iov = iov;

  sc = KSendMsg(&sb->msgport, vnode, &msg);

  if (sc == 0) {
    if (reply.args.write.nbytes_written >= 0) {
      if (offset != NULL) {
        *offset += reply.args.write.nbytes_written;
      }
    }    
  } else if (sc < 0) {
    goto exit;    
  }

  return reply.args.write.nbytes_written;

exit:
  return sc;

}

/*
 *
 */
int vfs_readdir(struct VNode *vnode, void *dst, size_t nbytes,
                              off64_t *cookie) {
  struct fsreq req;
  struct fsreply reply;
  struct SuperBlock *sb;
  struct Msg msg;
  struct IOV iov[3];
  int sc;
  int sz;
  
  Info ("vfs_readdir, dst = %08x, nbytes = %d", (vm_addr)dst, nbytes);
  memset(&req, 0, sizeof req);
  sb = vnode->superblock;

  LockSuperBlock(sb);

  req.cmd = CMD_READDIR;
  req.args.readdir.inode_nr = vnode->inode_nr;
  req.args.readdir.offset = *cookie;
  req.args.readdir.sz = nbytes;

  iov[0].addr = &req;
  iov[0].size = sizeof req;
  iov[1].addr = &reply;
  iov[1].size = sizeof reply;
  iov[2].addr = dst;
  iov[2].size = nbytes;

  msg.iov_cnt = 3;
  msg.iov = iov;

  sc = KSendMsg(&sb->msgport, vnode, &msg);

  if (sc < 0) {
    Info ("vfs_readdir sendmsg sc = %d", sc);
    sz = -1;    
    goto exit;
  }

  *cookie = reply.args.readdir.offset;
  sz = reply.args.readdir.nbytes_read;

  UnlockSuperBlock(sb);
//  Info ("vfs_readdir sz = %d", sz);
  return sz;

exit:
  UnlockSuperBlock(sb);
  return sz;
}




/*
 * TODO: Need to allocate vnode but not set INODE nr, then send message to server.
 * Otherwise could fail to allocate after sending.
 */
int vfs_mknod(struct VNode *dir, char *name, struct stat *stat,
                            struct VNode **result) {
  int status;
  struct SuperBlock *sb;
  struct fsreq req;
  struct fsreply reply;
  struct Msg msg;
  struct IOV iov[3];
  struct VNode *vnode = NULL;

  memset(&req, 0, sizeof req);
  sb = dir->superblock;

  LockSuperBlock(sb);

  req.cmd = CMD_MKNOD;
  req.args.mknod.dir_inode_nr = dir->inode_nr;
  req.args.mknod.name_sz = StrLen(name) + 1;
//  req.args.mknod.size = 0;
  req.args.mknod.uid = stat->st_uid;
  req.args.mknod.gid = stat->st_gid;
  req.args.mknod.mode = stat->st_mode;


  Info ("vfs_mknod, mode = %o", stat->st_mode);
  
  // TODO: Copy stat fields to req
  
  iov[0].addr = &req;
  iov[0].size = sizeof req;
  iov[1].addr = name;
  iov[1].size = req.args.mkdir.name_sz;
  iov[2].addr = &reply;
  iov[2].size = sizeof reply;

  msg.iov_cnt = 3;
  msg.iov = iov;

  status = KSendMsg(&sb->msgport, dir, &msg);

  if (status < 0) {
    goto exit;
  }

  vnode = VNodeNew(sb, reply.args.lookup.inode_nr);

  if (vnode == NULL) {
    status = -ENOMEM;
    goto exit;
  }

  vnode->nlink = 1;      
  vnode->reference_cnt = 1;
  vnode->size = reply.args.mknod.size;      
  vnode->uid = reply.args.mknod.uid;  
  vnode->gid = reply.args.mknod.gid;
  vnode->mode = reply.args.mknod.mode;
  vnode->flags = V_VALID;

  *result = vnode;

  status = reply.args.mknod.status;

  UnlockSuperBlock(sb);
  Info ("vfs_mknod success, mode = %o", vnode->mode);
  return status;

exit:
  UnlockSuperBlock(sb);
  Info ("vfs_mknod fail");
  return status;
  
}

/*
 *
 */
int vfs_mkdir(struct VNode *dir, char *name, struct stat *stat,
                            struct VNode **result) {
  return -ENOTSUP;
}

/*
 *
 */
int vfs_rmdir(struct VNode *dvnode, char *name) {
  int sc;
  struct SuperBlock *sb;
  struct fsreq req;
  struct fsreply reply;
  struct Msg msg;
  struct IOV iov[3];
  
  memset(&req, 0, sizeof req);
  sb = dvnode->superblock;

  LockSuperBlock(sb);

  req.cmd = CMD_RMDIR;
  req.args.rmdir.dir_inode_nr = dvnode->inode_nr;
  req.args.rmdir.name_sz = StrLen(name) + 1;

  iov[0].addr = &req;
  iov[0].size = sizeof req;
  iov[1].addr = &reply;
  iov[1].size = sizeof reply;
  iov[2].addr = name;
  iov[2].size = req.args.rmdir.name_sz;

  msg.iov_cnt = 3;
  msg.iov = iov;

  sc = KSendMsg(&sb->msgport, dvnode, &msg);

  if (sc < 0) {
    sc = -1;    
    goto exit;
  }

  sc = reply.args.rmdir.status;

  UnlockSuperBlock(sb);
  return sc;

exit:
  UnlockSuperBlock(sb);
  return sc;
}

/*
 *
 */
int vfs_truncate(struct VNode *vnode, size_t size) {
  struct fsreq req;
  struct fsreply reply;
  struct SuperBlock *sb;
  struct Msg msg;
  struct IOV iov[2];
  int sc;
  int sz;

  memset(&req, 0, sizeof req);
  sb = vnode->superblock;
  LockSuperBlock(sb);

  req.cmd = CMD_TRUNCATE;
  req.args.truncate.inode_nr = vnode->inode_nr;
  req.args.truncate.size = size;

  iov[0].addr = &req;
  iov[0].size = sizeof req;
  iov[1].addr = &reply;
  iov[1].size = sizeof reply;

  msg.iov_cnt = 2;
  msg.iov = iov;
  
  sc = KSendMsg(&sb->msgport, vnode, &msg);

  if (sc < 0) {
    sz = -1;
    goto exit;
  }

  sz = reply.args.truncate.status;

  UnlockSuperBlock(sb);
  return sz;

exit:
  UnlockSuperBlock(sb);
  return sz;
}

/*
 *
 */
int vfs_rename(struct VNode *src_dvnode, char *src_name,
                             struct VNode *dst_dvnode, char *dst_name) {
  return -ENOTSUP;
}

/*
 *
 */
int vfs_chmod(struct VNode *vnode, mode_t mode) {
  struct fsreq req;
  struct fsreply reply;
  struct SuperBlock *sb;
  struct Msg msg;
  struct IOV iov[2];
  int sc;
  
  memset(&req, 0, sizeof req);
  sb = vnode->superblock;

  LockSuperBlock(sb);

  req.cmd = CMD_CHMOD;
  req.args.chmod.inode_nr = vnode->inode_nr;
  req.args.chmod.mode = mode;

  iov[0].addr = &req;
  iov[0].size = sizeof req;
  iov[1].addr = &reply;
  iov[1].size = sizeof reply;

  msg.iov_cnt = 2;
  msg.iov = iov;

  sc = KSendMsg(&sb->msgport, vnode, &msg);

  if (sc < 0) {
    sc = -1;    
    goto exit;
  }

  sc = reply.args.chmod.status;

  UnlockSuperBlock(sb);
  return sc;

exit:
  UnlockSuperBlock(sb);
  return sc;
}

/*
 *
 */
int vfs_chown(struct VNode *vnode, uid_t uid, gid_t gid) {
  struct fsreq req;
  struct fsreply reply;
  struct SuperBlock *sb;
  struct Msg msg;
  struct IOV iov[2];
  int sc;
  
  memset(&req, 0, sizeof req);
  sb = vnode->superblock;

  LockSuperBlock(sb);

  req.cmd = CMD_CHOWN;
  req.args.chown.inode_nr = vnode->inode_nr;
  req.args.chown.uid = uid;
  req.args.chown.gid = gid;
  
  iov[0].addr = &req;
  iov[0].size = sizeof req;
  iov[1].addr = &reply;
  iov[1].size = sizeof reply;

  msg.iov_cnt = 2;
  msg.iov = iov;

  sc = KSendMsg(&sb->msgport, vnode, &msg);

  if (sc < 0) {
    sc = -1;    
    goto exit;
  }

  sc = reply.args.chown.status;

  UnlockSuperBlock(sb);
  return sc;

exit:
  UnlockSuperBlock(sb);
  return sc;
}

/*
 *
 */
int vfs_unlink(struct VNode *dvnode, char *name) {
  struct fsreq req;
  struct fsreply reply;
  struct SuperBlock *sb;
  struct Msg msg;
  struct IOV iov[3];
  int sc;
  
  memset(&req, 0, sizeof req);
  sb = dvnode->superblock;

  LockSuperBlock(sb);

  req.cmd = CMD_UNLINK;
  req.args.unlink.dir_inode_nr = dvnode->inode_nr;
  req.args.unlink.name_sz = StrLen(name) + 1;

  iov[0].addr = &req;
  iov[0].size = sizeof req;
  iov[1].addr = &reply;
  iov[1].size = sizeof reply;
  iov[2].addr = name;
  iov[2].size = req.args.unlink.name_sz;

  msg.iov_cnt = 3;
  msg.iov = iov;

  sc = KSendMsg(&sb->msgport, dvnode, &msg);

  if (sc < 0) {
    sc = -1;    
    goto exit;
  }

  sc = reply.args.unlink.status;

  UnlockSuperBlock(sb);
  return sc;

exit:
  UnlockSuperBlock(sb);
  return sc;
}

/*
 *
 */
int vfs_mklink(struct VNode *dvnode, char *name, char *link,
                             struct stat *stat) {
  return -ENOTSUP;
}

/*
 *
 */
int vfs_rdlink(struct VNode *vnode, char *buf, size_t sz) {
  return -ENOTSUP;
}



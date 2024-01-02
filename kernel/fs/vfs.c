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


/* @brief   Lookup a file within a directory
 *
 * @param   dvnode, directory in which to search
 * @param   name, filename to look for
 * @param   result, location to store vnode pointer of the found file
 * @return  0 on success, negative errno on failure
 */
int vfs_lookup(struct VNode *dvnode, char *name,
                             struct VNode **result) {
  struct SuperBlock *sb;
  struct VNode *vnode;
  struct fsreq req = {0};
  struct fsreply reply = {0};
  struct IOV siov[2];
  struct IOV riov[1];
  size_t name_sz;
  int sc;

  KASSERT(dvnode != NULL);
  KASSERT(name != NULL);
  KASSERT(result != NULL);  
  
  sb = dvnode->superblock;
  name_sz = StrLen(name) + 1;

  memset(&req, 0, sizeof req);

  req.cmd = CMD_LOOKUP;
  req.args.lookup.dir_inode_nr = dvnode->inode_nr;
  req.args.lookup.name_sz = name_sz;

  siov[0].addr = &req;
  siov[0].size = sizeof req;
  siov[1].addr = name;
  siov[1].size = name_sz;
  
  riov[0].addr = &reply;
  riov[0].size = sizeof reply;
  
  sc = ksendmsg(&sb->msgport, NELEM(siov), siov, NELEM(riov), riov);

  if (sc != 0) {
    *result = NULL;
    return sc;
  }
  
  if (reply.args.lookup.inode_nr == dvnode->inode_nr) {
    vnode_inc_ref(dvnode);
    vnode = dvnode;
  } else {
    vnode = vnode_get(dvnode->superblock, reply.args.lookup.inode_nr);
  }

  if (vnode == NULL) {
    vnode = vnode_new(sb, reply.args.lookup.inode_nr);

    if (vnode == NULL) {
      return -ENOMEM;
    }

    vnode->nlink = 1;           // FIXME: Need to get nlink. Do we have dev id in each vnode (from superblock)?
    vnode->reference_cnt = 1;
    vnode->size = reply.args.lookup.size;      
    vnode->uid = reply.args.lookup.uid;  
    vnode->gid = reply.args.lookup.gid;
    vnode->mode = reply.args.lookup.mode;
    vnode->flags = V_VALID;
  } 

  *result = vnode;
  return 0;
}


/* @brief   Create a file
 *
 * TODO: Merge with lookup.
 */
int vfs_create(struct VNode *dir, char *name, int oflags,
                             struct stat *stat, struct VNode **result)
{
  return -ENOTSUP;
}


/* @brief   Read from a file or a device
 */
ssize_t vfs_read(struct VNode *vnode, void *dst, size_t nbytes, off64_t *offset)
{
  struct SuperBlock *sb;
  struct fsreq req = {0};
  struct fsreply reply = {0};
  struct IOV siov[1];
  struct IOV riov[2];
  int nbytes_read;

  KASSERT(vnode != NULL);
  KASSERT(dst != NULL);
        
  sb = vnode->superblock;

  memset(&req, 0, sizeof req);
  
  req.cmd = CMD_READ;
  req.args.read.inode_nr = vnode->inode_nr;
  
  if (offset != NULL) {
    req.args.read.offset = *offset;
  } else {
    req.args.read.offset = 0;
  }

  req.args.read.sz = nbytes;
  
  siov[0].addr = &req;
  siov[0].size = sizeof req;

  riov[0].addr = &reply;
  riov[0].size = sizeof reply;
  riov[1].addr = dst;
  riov[1].size = nbytes;

  nbytes_read = ksendmsg(&sb->msgport, NELEM(siov), siov, NELEM(riov), riov);

  if (nbytes_read < 0) {
    return nbytes_read;    
  }

  if (offset != NULL) {
    *offset += nbytes_read;
  }

  return nbytes_read;
}


/* @brief   Synchronous write to a file or device
 */
ssize_t vfs_write(struct VNode *vnode, void *src, size_t nbytes, off64_t *offset)
{
  struct SuperBlock *sb;
  struct fsreq req = {0};
  struct fsreply reply = {0};
  struct IOV siov[2];
  struct IOV riov[1];
  int nbytes_written;

  sb = vnode->superblock;

  memset(&req, 0, sizeof req);

  req.cmd = CMD_WRITE;
  req.args.write.inode_nr = vnode->inode_nr;

  if (offset != NULL) {
    req.args.write.offset = *offset;
  } else {
    req.args.write.offset = 0;
  }

  req.args.write.sz = nbytes;

  siov[0].addr = &req;
  siov[0].size = sizeof req;
  siov[1].addr = src;
  siov[1].size = nbytes;

  riov[0].addr = &reply;
  riov[0].size = sizeof reply;

  nbytes_written = ksendmsg(&sb->msgport, NELEM(siov), siov, NELEM(riov), riov);
  
  if (nbytes_written < 0) {
    return nbytes_written;  
  }
      
  if (offset != NULL) {
    *offset += nbytes_written;
  }

  return nbytes_written;
}


/* @brief   Write a cached file block asynchronously
 */
int vfs_write_async(struct SuperBlock *sb, struct DelWriMsg *msg, struct Buf *buf)
{
  struct VNode *vnode;
  size_t nbytes;
  int sc;

  vnode = buf->vnode;
  
  if (vnode->size - buf->cluster_offset < CLUSTER_SZ) {
    nbytes = vnode->size - buf->cluster_offset;
  } else {
    nbytes = CLUSTER_SZ;
  }    
  
  memset(&msg->req, 0, sizeof msg->req);

  msg->req.cmd = CMD_WRITE;
  msg->req.args.write.inode_nr = vnode->inode_nr;

  msg->req.args.write.offset = buf->cluster_offset;
  msg->req.args.write.sz = nbytes;

  msg->siov[0].addr = &msg->req;
  msg->siov[0].size = sizeof msg->req;
  msg->siov[1].addr = buf->data;
  msg->siov[1].size = nbytes;

  msg->riov[0].addr = &msg->reply;
  msg->riov[0].size = sizeof msg->reply;

  sc = ksendmsg(&sb->msgport, NELEM(msg->siov), msg->siov, NELEM(msg->riov), msg->riov);
  return sc;
}


/*
 *
 */
int vfs_readdir(struct VNode *vnode, void *dst, size_t nbytes, off64_t *cookie)
{
  struct fsreq req = {0};
  struct fsreply reply = {0};
  struct SuperBlock *sb;
  struct IOV siov[1];
  struct IOV riov[2];
  int nbytes_read;

  sb = vnode->superblock;

  memset(&req, 0, sizeof req);

  req.cmd = CMD_READDIR;
  req.args.readdir.inode_nr = vnode->inode_nr;
  req.args.readdir.offset = *cookie;
  req.args.readdir.sz = nbytes;

  siov[0].addr = &req;
  siov[0].size = sizeof req;
  
  riov[0].addr = &reply;
  riov[0].size = sizeof reply;
  riov[1].addr = dst;
  riov[1].size = nbytes;

  nbytes_read = ksendmsg(&sb->msgport, NELEM(siov), siov, NELEM(riov), riov);
  
  if (nbytes_read < 0) {
    return nbytes_read;
  }

  *cookie = reply.args.readdir.offset;
  return nbytes_read;
}


/*
 * FIXME: Need to allocate vnode but not set INODE nr, then send message to server.
 * Otherwise could fail to allocate after sending.
 */
int vfs_mknod(struct VNode *dir, char *name, struct stat *stat, struct VNode **result)
{
  struct fsreq req = {0};
  struct fsreply reply = {0};
  struct SuperBlock *sb;
  struct IOV siov[2];
  struct IOV riov[1];
  struct VNode *vnode = NULL;
  int sc;

  sb = dir->superblock;

  memset(&req, 0, sizeof req);

  req.cmd = CMD_MKNOD;
  req.args.mknod.dir_inode_nr = dir->inode_nr;
  req.args.mknod.name_sz = StrLen(name) + 1;
  // req.args.mknod.size = 0;
  req.args.mknod.uid = stat->st_uid;
  req.args.mknod.gid = stat->st_gid;
  req.args.mknod.mode = stat->st_mode;
  
  // TODO: Copy stat fields to req
  
  siov[0].addr = &req;
  siov[0].size = sizeof req;
  siov[1].addr = name;
  siov[1].size = req.args.mkdir.name_sz;
  
  riov[0].addr = &reply;
  riov[0].size = sizeof reply;

  sc = ksendmsg(&sb->msgport, NELEM(siov), siov, NELEM(riov), riov);
  
  if (sc < 0) {
    return sc;
  }

  vnode = vnode_new(sb, reply.args.lookup.inode_nr);

  if (vnode == NULL) {
    return -ENOMEM;
  }

  vnode->nlink = 1;      
  vnode->reference_cnt = 1;
  vnode->size = reply.args.mknod.size;      
  vnode->uid = reply.args.mknod.uid;  
  vnode->gid = reply.args.mknod.gid;
  vnode->mode = reply.args.mknod.mode;
  vnode->flags = V_VALID;

  *result = vnode;
  return sc;
}


/*
 *
 */
int vfs_mkdir(struct VNode *dir, char *name, struct stat *stat, struct VNode **result)
{
  return -ENOTSUP;
}


/*
 *
 */
int vfs_rmdir(struct VNode *dvnode, char *name)
{
  struct fsreq req = {0};
  struct fsreply reply = {0};
  struct SuperBlock *sb;
  struct IOV siov[2];
  struct IOV riov[1];
  int sc;
  
  sb = dvnode->superblock;

  memset(&req, 0, sizeof req);

  req.cmd = CMD_RMDIR;
  req.args.rmdir.dir_inode_nr = dvnode->inode_nr;
  req.args.rmdir.name_sz = StrLen(name) + 1;

  siov[0].addr = &req;
  siov[0].size = sizeof req;
  siov[2].addr = name;
  siov[2].size = req.args.rmdir.name_sz;

  riov[0].addr = &reply;
  riov[0].size = sizeof reply;

  sc = ksendmsg(&sb->msgport, NELEM(siov), siov, NELEM(riov), riov);  
  return sc;
}


/*
 *
 */
int vfs_truncate(struct VNode *vnode, size_t size)
{
  struct fsreq req = {0};
  struct fsreply reply = {0};
  struct SuperBlock *sb;
  struct IOV siov[1];
  struct IOV riov[1];  
  int sc;
  int sz;

  sb = vnode->superblock;

  memset(&req, 0, sizeof req);

  req.cmd = CMD_TRUNCATE;
  req.args.truncate.inode_nr = vnode->inode_nr;
  req.args.truncate.size = size;

  siov[0].addr = &req;
  siov[0].size = sizeof req;
  
  riov[0].addr = &reply;
  riov[0].size = sizeof reply;

  sc = ksendmsg(&sb->msgport, NELEM(siov), siov, NELEM(riov), riov);
  return sc;
}


/*
 *
 */
int vfs_rename(struct VNode *src_dvnode, char *src_name,
               struct VNode *dst_dvnode, char *dst_name)
{
  return -ENOTSUP;
}


/*
 *
 */
int vfs_chmod(struct VNode *vnode, mode_t mode)
{
  struct fsreq req = {0};
  struct fsreply reply = {0};
  struct SuperBlock *sb;
  struct IOV siov[1];
  struct IOV riov[1];
  int sc;
  
  sb = vnode->superblock;

  memset(&req, 0, sizeof req);

  req.cmd = CMD_CHMOD;
  req.args.chmod.inode_nr = vnode->inode_nr;
  req.args.chmod.mode = mode;

  siov[0].addr = &req;
  siov[0].size = sizeof req;
  
  riov[0].addr = &reply;
  riov[0].size = sizeof reply;

  sc = ksendmsg(&sb->msgport, NELEM(siov), siov, NELEM(riov), riov);
  return sc;
}


/*
 *
 */
int vfs_chown(struct VNode *vnode, uid_t uid, gid_t gid)
{
  struct fsreq req = {0};
  struct fsreply reply = {0};
  struct SuperBlock *sb;
  struct IOV siov[1];
  struct IOV riov[1];
  int sc;
  
  sb = vnode->superblock;

  memset(&req, 0, sizeof req);

  req.cmd = CMD_CHOWN;
  req.args.chown.inode_nr = vnode->inode_nr;
  req.args.chown.uid = uid;
  req.args.chown.gid = gid;
  
  siov[0].addr = &req;
  siov[0].size = sizeof req;
  
  riov[0].addr = &reply;
  riov[0].size = sizeof reply;
  
  sc = ksendmsg(&sb->msgport, NELEM(siov), siov, NELEM(riov), riov);
  return sc;
}


/*
 * Does this call vnode_put, or is it higher layer 
 * is the put done after the vfs_unlink ?  We should be the only reference
 * when this is called.
 */
int vfs_unlink(struct VNode *dvnode, char *name)
{
  struct fsreq req = {0};
  struct fsreply reply = {0};
  struct SuperBlock *sb;
  struct IOV siov[2];
  struct IOV riov[1];
  int sc;
  
  sb = dvnode->superblock;

  memset(&req, 0, sizeof req);

  req.cmd = CMD_UNLINK;
  req.args.unlink.dir_inode_nr = dvnode->inode_nr;
  req.args.unlink.name_sz = StrLen(name) + 1;

  siov[0].addr = &req;
  siov[0].size = sizeof req;
  siov[2].addr = name;
  siov[2].size = req.args.unlink.name_sz;

  riov[0].addr = &reply;
  riov[0].size = sizeof reply;
  
  sc = ksendmsg(&sb->msgport, NELEM(siov), siov, NELEM(riov), riov);
  return sc;
}


/*
 *
 */
int vfs_mklink(struct VNode *dvnode, char *name, char *link, struct stat *stat)
{
  return -ENOTSUP;
}


/*
 *
 */
int vfs_rdlink(struct VNode *vnode, char *buf, size_t sz)
{
  return -ENOTSUP;
}


/*
 *
 */
int vfs_fsync(struct VNode *vnode)
{
  return -ENOTSUP;
}



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

//#define KDEBUG 1

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
int vfs_lookup(struct VNode *dvnode, char *name, struct VNode **result) 
{
  struct SuperBlock *sb;
  struct VNode *vnode;
  struct fsreq req = {0};
  struct fsreply reply = {0};
  struct IOV siov[2];
  struct IOV riov[1];
  size_t name_sz;
  int sc;

//  Info("vfs_lookup(dvn:%08x, name: %s)",(uint32_t)dvnode, name);

  KASSERT(dvnode != NULL);
  KASSERT(name != NULL);
  KASSERT(result != NULL);  
  
  sb = dvnode->superblock;
  name_sz = StrLen(name) + 1;

  req.cmd = CMD_LOOKUP;
  req.args.lookup.dir_inode_nr = dvnode->inode_nr;
  req.args.lookup.name_sz = name_sz;

  siov[0].addr = &req;
  siov[0].size = sizeof req;
  siov[1].addr = name;
  siov[1].size = name_sz;
  
  riov[0].addr = &reply;
  riov[0].size = sizeof reply;
  
#if 0
  reply.args.lookup.inode_nr = 0xbeefdead;
  reply.args.lookup.size = 0xdeadbeef;
#endif
  
  sc = ksendmsg(&sb->msgport, NELEM(siov), siov, NELEM(riov), riov);

  if (sc != 0) {
    Info("ksendmsg failed, sc:%d", sc);
    *result = NULL;
    return sc;
  }
  
//  Info("vfs_lookup reply, inode_nr=%d", reply.args.lookup.inode_nr);
//  Info("vfs_lookup reply, size = %d", (uint32_t)reply.args.lookup.size);
  
  if (reply.args.lookup.inode_nr == dvnode->inode_nr) {
    vnode_inc_ref(dvnode);
    vnode = dvnode;
  } else {
    vnode = vnode_get(dvnode->superblock, reply.args.lookup.inode_nr);
  }

  if (vnode == NULL) {
    vnode = vnode_new(sb, reply.args.lookup.inode_nr);

    if (vnode == NULL) {
      Info("vfs_lookup, vnode_new -ENOMEM");
      return -ENOMEM;
    }

    vnode->nlink = 1;           // FIXME: Need to get nlink.
    vnode->reference_cnt = 1;
    vnode->size = reply.args.lookup.size;      
    vnode->uid = reply.args.lookup.uid;  
    vnode->gid = reply.args.lookup.gid;
    vnode->mode = reply.args.lookup.mode;
    vnode->flags = V_VALID;
  } 

//  Info("vfs_lookup result: vnode:%08x, ino:%d", (uint32_t)vnode, vnode->inode_nr);
  *result = vnode;
  return 0;
}


/* @brief   Create a file
 *
 * TODO: Merge with lookup.
 */
int vfs_create(struct VNode *dvnode, char *name, int oflags,
                             struct stat *stat, struct VNode **result)
{
  struct SuperBlock *sb;
  struct VNode *vnode;
  struct fsreq req = {0};
  struct fsreply reply = {0};
  struct IOV siov[2];
  struct IOV riov[1];
  size_t name_sz;
  int sc;

  Info("vfs_create(dvn:%08x, name: %s)",(uint32_t)dvnode, name);

  KASSERT(dvnode != NULL);
  KASSERT(name != NULL);
  KASSERT(result != NULL);  
  
  sb = dvnode->superblock;
  name_sz = StrLen(name) + 1;

  req.cmd = CMD_CREATE;
  req.args.create.dir_inode_nr = dvnode->inode_nr;
  req.args.create.name_sz = name_sz;
  req.args.create.oflags = oflags;
  req.args.create.mode = stat->st_mode;
  req.args.create.uid = stat->st_uid;
  req.args.create.gid = stat->st_gid;

  siov[0].addr = &req;
  siov[0].size = sizeof req;
  siov[1].addr = name;
  siov[1].size = name_sz;
  
  riov[0].addr = &reply;
  riov[0].size = sizeof reply;

#if 0  
  reply.args.create.inode_nr = 0xbeefdead;
  reply.args.create.size = 0xdeadbeef;
#endif
  
  sc = ksendmsg(&sb->msgport, NELEM(siov), siov, NELEM(riov), riov);

  if (sc != 0) {
    Info("vfs_create: ksendmsg failed, sc:%d", sc);
    *result = NULL;
    return sc;
  }
  
  Info("vfs_create reply, inode_nr=%d", reply.args.create.inode_nr);

  vnode = vnode_new(sb, reply.args.create.inode_nr);

  if (vnode == NULL) {
    Info("vfs_create, vnode_new -ENOMEM");
    return -ENOMEM;
  }

  vnode->nlink = 1;           // FIXME: Need to get nlink. 
  vnode->reference_cnt = 1;
  vnode->size = reply.args.create.size;      
  vnode->uid = reply.args.create.uid;  
  vnode->gid = reply.args.create.gid;
  vnode->mode = reply.args.create.mode;
  vnode->flags = V_VALID;

  Info("vfs_create result: vnode:%08x, ino:%d", (uint32_t)vnode, vnode->inode_nr);
  *result = vnode;
  return 0;
}


/* @brief   Read from a file or a device
 */
ssize_t vfs_read(struct VNode *vnode, void *dst, size_t nbytes, off64_t *offset)
{
  struct SuperBlock *sb;
  struct fsreq req = {0};
  struct IOV siov[1];
  struct IOV riov[1];
  int nbytes_read;

  KASSERT(vnode != NULL);
  KASSERT(dst != NULL);
        
  sb = vnode->superblock;

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
  riov[0].addr = dst;
  riov[0].size = nbytes;

  nbytes_read = ksendmsg(&sb->msgport, NELEM(siov), siov, NELEM(riov), riov);

  if (nbytes_read < 0) {
    Info("vfs_read error :%d", nbytes_read);
    return nbytes_read;    
  }

  if (offset != NULL) {
    *offset += nbytes_read;
  }

//  Info("vfs_read nbytes_read:%d", nbytes_read);
  return nbytes_read;
}


/* @brief   Synchronous write to a file or device
 */
ssize_t vfs_write(struct VNode *vnode, void *src, size_t nbytes, off64_t *offset)
{
  struct SuperBlock *sb;
  struct fsreq req = {0};
  struct IOV siov[2];
  int nbytes_written;

  Info("vfs_write nbytes:%d, offset:%08x", nbytes, (uint32_t)offset);

  sb = vnode->superblock;

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

  nbytes_written = ksendmsg(&sb->msgport, NELEM(siov), siov, 0, NULL);
  
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
int vfs_write_async(struct SuperBlock *sb, struct Buf *buf)
{
  struct VNode *vnode;
  size_t nbytes;
  int sc;
	struct Process *current;
	
  Info("vfs_write_async buf:%08x, offset:%08x", (uint32_t)buf, (uint32_t)buf->cluster_offset);
  
  current = get_current_process();
    
  vnode = buf->vnode;
  
  if (vnode->size - buf->cluster_offset < CLUSTER_SZ) {
    nbytes = vnode->size - buf->cluster_offset;
  } else {
    nbytes = CLUSTER_SZ;
  }    
  
  memset(&buf->req, 0, sizeof buf->req);
  buf->req.cmd = CMD_WRITE;
  buf->req.args.write.inode_nr = vnode->inode_nr;
  buf->req.args.write.offset = buf->cluster_offset;
  buf->req.args.write.sz = nbytes;

  buf->siov[0].addr = &buf->req;
  buf->siov[0].size = sizeof buf->req;
  buf->siov[1].addr = buf->data;
  buf->siov[1].size = nbytes;

  buf->msg.reply_port = NULL;  
  buf->msg.siov_cnt = NELEM(buf->siov);
  buf->msg.siov = buf->siov;
  buf->msg.riov_cnt = 0;
  buf->msg.riov = NULL;  
  buf->msg.reply_status = 0;
	
  sc = kputmsg(&sb->msgport, (struct Msg *)buf);
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
  siov[1].size = req.args.mknod.name_sz;
  
  riov[0].addr = &reply;
  riov[0].size = sizeof reply;

  sc = ksendmsg(&sb->msgport, NELEM(siov), siov, NELEM(riov), riov);
  
  if (sc < 0) {
    return sc;
  }

  vnode = vnode_new(sb, reply.args.mknod.inode_nr);

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
  struct fsreq req = {0};
  struct fsreply reply = {0};
  struct SuperBlock *sb;
  struct IOV siov[2];
  struct IOV riov[1];
  struct VNode *vnode = NULL;
  int sc;

  sb = dir->superblock;

  req.cmd = CMD_MKDIR;
  req.args.mkdir.dir_inode_nr = dir->inode_nr;
  req.args.mkdir.name_sz = StrLen(name) + 1;
  // req.args.mknod.size = 0;
  req.args.mkdir.uid = stat->st_uid;
  req.args.mkdir.gid = stat->st_gid;
  req.args.mkdir.mode = stat->st_mode;
  
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

  vnode = vnode_new(sb, reply.args.mkdir.inode_nr);

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
int vfs_rmdir(struct VNode *dvnode, char *name)
{
  struct fsreq req = {0};
  struct fsreply reply = {0};
  struct SuperBlock *sb;
  struct IOV siov[2];
  struct IOV riov[1];
  int sc;
  
  sb = dvnode->superblock;

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
	Error("vfs_mklink -ENOTSUP");
  return -ENOTSUP;
}


/*
 *
 */
int vfs_rdlink(struct VNode *vnode, char *buf, size_t sz)
{
	Error("vfs_rdlink -ENOTSUP");
  return -ENOTSUP;
}


/*
 *
 */
int vfs_fsync(struct VNode *vnode)
{
	Error("vfs_fsync -ENOTSUP");
  return -ENOTSUP;
}


/*
 *
 */
int vfs_isatty(struct VNode *vnode)
{
  struct fsreq req = {0};
  struct SuperBlock *sb;
  struct IOV siov[1];
  int sc;
  
  sb = vnode->superblock;

  req.cmd = CMD_ISATTY;
  req.args.isatty.inode_nr = vnode->inode_nr;

  siov[0].addr = &req;
  siov[0].size = sizeof req;
  
  sc = ksendmsg(&sb->msgport, NELEM(siov), siov, 0, NULL);
  return sc;
}


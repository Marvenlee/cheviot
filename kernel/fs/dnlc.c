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

/* @brief   Functions for using the Directory Name Lookup Cache
 *          FIXME: These have been removed until vnodes and their reference counting
 *          have been debugged.
 */

//#define KDEBUG

#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <sys/mount.h>


/* @brief   Lookup a file in the Directory Name Lookup Cache
 *
 * Looks up a vnode in the DNLC based on parent directory vnode
 * and the filename. Stores the resulting vnode pointer in vnp.
 *
 * NOTE: For mount points or devices the "covered vnode" should be stored.
 *
 * TODO:  Add LOOKUP_NOCACHE flag handling  (should be in superblock->flags)
 */

int dname_lookup(struct VNode *dir, char *name, struct VNode **vnp)
{
  int t;
  int sz;
  int key;
  struct DName *dname;

  if (dir->superblock->flags & MNT_NODNLC) {
    *vnp = NULL;
    return -1;
  }

  if ((sz = StrLen(name) + 1) > DNAME_SZ) {
    *vnp = NULL;
    return -1;
  }

  for (key = 0, t = 0; t < sz && name[t] != '\0'; t++) {
    key += name[t];
  }
  
  key %= DNAME_HASH;

  dname = LIST_HEAD(&dname_hash[key]);

  while (dname != NULL) {
    if (dname->dir_vnode == dir && StrCmp(dname->name, name) == 0) {
      // TODO: Replace with inode_nr, do a VNodeGet
      *vnp = dname->vnode;
      vnode_inc_ref(*vnp);
      return 0;
    }

    dname = LIST_NEXT(dname, hash_link);
  }

  *vnp = NULL;
  return -1;
}


/* @brief   Add a filename and associated vnode to the Directory Name Lookup Cache
 *
 * Replaces existing entry, useful when negative caching and removing or adding file.
 * TODO: (Perhaps assert we are not replacing existing vnode with another valid
 * vnode).
 */
int dname_enter(struct VNode *dir, struct VNode *vn, char *name)
{
  int t;
  int sz;
  int key;
  struct DName *dname;

  if (dir->superblock->flags & MNT_NODNLC) {
    return -1;
  }

  if ((sz = StrLen(name) + 1) > DNAME_SZ) {
    return -1;
  }
  
  for (key = 0, t = 0; t < sz && name[t] != '\0'; t++) {
    key += name[t];
  }
  
  key %= DNAME_HASH;

  dname = LIST_HEAD(&dname_hash[key]);

  while (dname != NULL) {
    if (dname->dir_vnode == dir && StrCmp(dname->name, name) == 0) {
      dname->hash_key = key;
      dname->dir_vnode = dir;
      dname->vnode = vn;
      return 0;
    }

    dname = LIST_NEXT(dname, hash_link);
  }

  dname = LIST_HEAD(&dname_lru_list);

  LIST_REM_HEAD(&dname_lru_list, lru_link);

  if (dname->hash_key != -1) {
    LIST_REM_ENTRY(&dname_hash[dname->hash_key], dname, hash_link);
  }

  dname->hash_key = key;
  dname->dir_vnode = dir;
  dname->vnode = vn;
  StrLCpy(dname->name, name, DNAME_SZ);

  LIST_ADD_TAIL(&dname_lru_list, dname, lru_link);
  LIST_ADD_HEAD(&dname_hash[key], dname, hash_link);
  return 0;
}


/* @brief   Remove an entry from the Directory Name Lookup Cache
 */
int dname_remove(struct VNode *dir, char *name)
{
  int t;
  int sz;
  int key;
  struct DName *dname;
  
  if (dir->superblock->flags & MNT_NODNLC) {
    return -1;
  }

  if ((sz = StrLen(name) + 1) > DNAME_SZ) {
    return -1;
  }

  for (key = 0, t = 0; t < sz && name[t] != '\0'; t++) {
    key += name[t];
  }

  key %= DNAME_HASH;
  dname = LIST_HEAD(&dname_hash[key]);

  while (dname != NULL) {
    if (StrCmp(dname->name, name) == 0) {
      LIST_REM_ENTRY(&dname_hash[key], dname, hash_link);
      dname->hash_key = -1;
      LIST_REM_ENTRY(&dname_lru_list, dname, lru_link);
      LIST_ADD_HEAD(&dname_lru_list, dname, lru_link);
      return 0;
    }

    dname = LIST_NEXT(dname, hash_link);
  }

  return -1;
}


/* @brief   Remove all entries in the DNLC that reference a specific vnode
 *
 * Removes any DNLC entry associated with a vnode, whether it is
 * a directory_vnode or the vnode it points to,
 * For example "/dir" "/dir/." "/dir/another/.." all point to same vnode
 */
void dname_purge_vnode(struct VNode *vnode)
{
  int t;

  for (t = 0; t < NR_DNAME; t++) {
    if (dname_table[t].hash_key != -1 &&
        (dname_table[t].vnode == vnode || dname_table[t].dir_vnode == vnode)) {
      LIST_REM_ENTRY(&dname_hash[dname_table[t].hash_key], &dname_table[t],
                     hash_link);
      dname_table[t].hash_key = -1;
      LIST_REM_ENTRY(&dname_lru_list, &dname_table[t], lru_link);
      LIST_ADD_HEAD(&dname_lru_list, &dname_table[t], lru_link);
    }
  }
}


/* @brief   Remove all DNLC entries related to a specific superblock
 *
 * @param   sb,
 */
void dname_purge_superblock(struct SuperBlock *sb)
{
  for (int t = 0; t < NR_DNAME; t++) {
    if (dname_table[t].hash_key != -1 &&
        (dname_table[t].vnode->superblock == sb)) {
      LIST_REM_ENTRY(&dname_hash[dname_table[t].hash_key], &dname_table[t],
                     hash_link);
      dname_table[t].hash_key = -1;
      LIST_REM_ENTRY(&dname_lru_list, &dname_table[t], lru_link);
      LIST_ADD_HEAD(&dname_lru_list, &dname_table[t], lru_link);
    }
  }
}


/* @brief   Remove all entries from the Directory Name Lookup Cache
 *
 * To be used if filesystem hierarchy dramatically alters, e.g. when
 * sys_pivotroot() is called.
 */
void dname_purge_all(void)
{
  for (int t = 0; t < NR_DNAME; t++) {
    dname_table[t].hash_key = -1;
    dname_table[t].vnode = NULL;
  }

  LIST_INIT(&dname_lru_list);
}


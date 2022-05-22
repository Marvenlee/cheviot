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

//#define KDEBUG

#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <string.h>

// Static Prototypes
static int LookupPath(struct Lookup *lookup, int flags);
static int FollowLink(struct Lookup *lookup, struct VNode *vnode);
static char *PathToken(struct Lookup *lookup);
static struct VNode *Advance(struct VNode **cur_vnode, char *component,
                             struct Lookup *lookup);

/*
 * TODO: Need to handle chroot, don't allow paths to go above process's root
 */
int Lookup(char *_path, int flags, struct Lookup *lookup) {
  struct Process *current;
  int err;
  int last;

  current = GetCurrentProcess();
  memset(lookup, 0, sizeof *lookup);
  lookup->vnode = NULL;
  lookup->parent = NULL;
  lookup->position = lookup->path;
  lookup->flags = flags;
  lookup->isLastName = FALSE;

  if (flags & LOOKUP_KERNEL) {
    StrLCpy(lookup->path, _path, sizeof lookup->path);
  } else if (CopyInString(lookup->path, _path, sizeof lookup->path) == -1) {
    return -ENAMETOOLONG;   // FIXME:  Could be EFAULT 
  }

  Info("Lookup");
  Info(".. lookup->path: %s", lookup->path);

  // Remove trailing slashes  // Or do we append "." to it if last char is a
  // slash?
  last = StrLen(lookup->path);
  while (last >= 0 && lookup->path[last] == '/') {
    lookup->path[last] = '\0';
    last--;
  }

  if (StrCmp(lookup->path, "/") == 0) {
    if (LOOKUP_TYPE(flags) & (LOOKUP_PARENT | LOOKUP_REMOVE)) {
      err = -EINVAL;
      goto exit;
    }

    VNodeIncRef(root_vnode);
    lookup->parent = NULL;
    lookup->vnode = root_vnode;
    lookup->last_component = &lookup->path[1];
  } else {
    lookup->start_vnode =
        (lookup->path[0] == '/') ? root_vnode->vnode_mounted_here : current->current_dir;

    if (lookup->start_vnode == NULL) {
      Info("LookupPath failed, no start vnode");
      err = -EINVAL;
      goto exit;
    }

    if ((err = LookupPath(lookup, flags)) != 0) {
      Info ("LookupPath failed, status = %d", err);
      goto exit;
    }
  }
  
  Info ("Lookup succeeded");
  return 0;

exit:
  return err;
}

// This could do combined lookup last dir and component
static int LookupPath(struct Lookup *lookup, int flags) {
  struct VNode *cur_vnode;
  struct VNode *vnode;
  char *name = NULL;
  int status;

  Info ("LookupPath");

  KASSERT(lookup->start_vnode != NULL);

  cur_vnode = lookup->start_vnode;
  VNodeIncRef(cur_vnode);

  while (1) {
    name = PathToken(lookup);

    KASSERT(name != NULL);

    Info("Lookup path token: %s", name);

    // Check if last component is . or .. when needing parent/component pair and
    // fail if so.
    if (lookup->isLastName == TRUE &&
        (LOOKUP_TYPE(flags) & (LOOKUP_PARENT | LOOKUP_REMOVE)) &&
        (StrCmp(name, ".") == 0 || StrCmp(name, "..") == 0)) {
      VNodePut(cur_vnode);
      lookup->parent = NULL;
      lookup->vnode = NULL;
      lookup->last_component = NULL;
      return -EINVAL;
    }

    vnode = Advance(&cur_vnode, name, lookup);
    
    Info ("vnode = %08x = Advance(%s)", (vm_addr)vnode, name);
    
    if (vnode == NULL) {
      // Is it a LOOKUP_PARENT/create and last component ?
      if (lookup->isLastName == TRUE && (LOOKUP_TYPE(flags) & LOOKUP_PARENT)) {
        lookup->parent = cur_vnode;
        lookup->vnode = NULL;
        lookup->last_component = name;
        Info ("Advance ret NULL, but LastName and LOOKUP Parent, Lookup OK");
        return 0;
      } else {
        lookup->parent = NULL;
        lookup->vnode = NULL;
        lookup->last_component = NULL;
        VNodePut(cur_vnode);
        Info ("LookupPath -ENOENT");
        return -ENOENT;
      }
    }

    if (S_ISLNK(vnode->mode)) {
      if (lookup->isLastName == TRUE && flags == LOOKUP_NOFOLLOW) {
        lookup->vnode = vnode;
        lookup->parent = cur_vnode;
        lookup->last_component = name;
        return 0;
      }

      status = FollowLink(lookup, vnode);
      if (status != 0) {
        VNodePut(cur_vnode);
        VNodePut(vnode);
        lookup->vnode = NULL;
        lookup->parent = NULL;
        lookup->last_component = NULL;
        return status;
      }

      VNodePut(vnode);
      // TODO: Determine if abs/relative path,  release /acquire root
      //                VNodePut(cur_vnode);
      //                cur_vnode = new_cur_vnode;
    } else if (S_ISDIR(vnode->mode)) {
      if (lookup->isLastName == TRUE) {
        // decide if we want parent or not

        if (LOOKUP_TYPE(flags) & (LOOKUP_PARENT | LOOKUP_REMOVE)) {
          lookup->vnode = vnode;
          lookup->parent = cur_vnode;
          lookup->last_component = name;
          return 0;
        } else {
          VNodePut(cur_vnode);
          lookup->vnode = vnode;
          lookup->parent = NULL;
          lookup->last_component = name;
          return 0;
        }
      }

      VNodePut(cur_vnode);

      // New vnode should be locked
      cur_vnode = vnode;
    } else if (S_ISREG(vnode->mode) || S_ISCHR(vnode->mode) || S_ISBLK(vnode->mode)) {
      if (lookup->isLastName == TRUE) {
        if (LOOKUP_TYPE(flags) & (LOOKUP_PARENT | LOOKUP_REMOVE)) {
          lookup->vnode = vnode;
          lookup->parent = cur_vnode;
          lookup->last_component = name;
          return 0;
        } else {
          VNodePut(cur_vnode);
          lookup->vnode = vnode;
          lookup->parent = NULL;
          lookup->last_component = name;
          return 0;
        }
      } else {
        VNodePut(cur_vnode);
        VNodePut(vnode);
        lookup->vnode = NULL;
        lookup->parent = NULL;
        lookup->last_component = NULL;
        return -ENOTDIR;
      }
    } else {
      KASSERT(0);      
    }
  }
}

/*
 * Copy the next component from the remaining pathname.
 * Returns a pointer to the component after the next component.
 */

static char *PathToken(struct Lookup *lookup) {
  char *ch;
  char *name;

  ch = lookup->position;

  if (ch == NULL) {
    return NULL;
  }

  while (*ch == '/') {
    ch++;
  }

  if (*ch == '\0') {
    return NULL;
  }

  name = ch;

  while (*ch != '/' && *ch != '\0') {
    ch++;
  }

  if (*ch == '\0') {
    lookup->position = NULL;
    lookup->isLastName = TRUE;
  } else {
    *ch = '\0';
    lookup->position = ch + 1;
    lookup->isLastName = FALSE;
  }

  return name;
}

/*
 * Looks up the component in the current vnode's directory. Return the vnode of
 * the
 * component we lookup.
 */

static struct VNode *Advance(struct VNode **_cur_vnode, char *component,
                             struct Lookup *lookup) {
  struct VNode *cur_vnode;
  struct VNode *new_cur_vnode;
  struct VNode *new_new_vnode;
  struct VNode *new_vnode;

  Info ("Advance : %s", component);
  
  cur_vnode = *_cur_vnode;


  // Is this test needed?
  if (cur_vnode == NULL || component == NULL || component[0] == '\0') {
    Info ("Advance failed, no vnode or component");
    return NULL;
  }
  
  
  
  // TODO: Handle covered vnode before lookup?
  


  Info (".. cur_vnode->inode_nr = %d",cur_vnode->inode_nr);

  if (cur_vnode == cur_vnode->superblock->root) {
    if (component[0] == '.' && component[1] == '.' && component[2] == '\0') {
      Info ("Component is ..");
      
      if (cur_vnode->vnode_covered == NULL) {
        Info ("Advance() failed, vnode->covered == NULL");
        return NULL;
      }
      
      Info ("vnode is covered");

      new_cur_vnode = cur_vnode->vnode_covered;
      VNodeIncRef(cur_vnode);
      VNodePut(cur_vnode);
      cur_vnode = new_cur_vnode;
      Info ("cur_vnode is covered, switching to it");
    }
    
    Info ("cur_vnode == superblock->root");
  }

  Info("vfs_lookup");
  
  if (vfs_lookup(cur_vnode, component, &new_vnode) != 0) {
    Info("(Advance - vfs_lookup %s failed", component);
    Info (".. (check) cur_vnode->inode_nr = %d",cur_vnode->inode_nr);

    return NULL;
  }

  if (new_vnode->vnode_mounted_here != NULL) {
    Info("*** new_vnode is a covered vnode ** mode = %o", new_vnode->mode);

    new_new_vnode = new_vnode->vnode_mounted_here;
    VNodePut(new_vnode);

    // FIXME: lookup parent never set until end, so why compare?
    // Perhaps update **cur_vnode ?
    /*
    if (lookup->parent == cur_vnode) {
        lookup->parent = cur_vnode->vnode_mounted_here;
    }
    */
    new_vnode = new_new_vnode;
    VNodeIncRef(new_vnode);
  }

  /*
      if (DNameLookup(cur_vnode, component, &new_vnode) != 0)
      {
          if (vfs_lookup(cur_vnode, component, &new_vnode) != 0) {
              DNameEnter (cur_vnode, NULL, component);
              return NULL;
          }

          DNameEnter (cur_vnode, new_vnode, component);
      }
  */

  /*
      if (new_vnode->vnode_mounted_here != NULL) {
          new_new_vnode = new_vnode->vnode_mounted_here;
          VNodePut(new_vnode);
          new_vnode = new_new_vnode;
          VNodeIncRef(new_vnode);
      }
  */
  
  Info ("Advance:vnode %08x", (uint32_t)new_vnode);

  return new_vnode;
}

/*
 *
 */

static int FollowLink(struct Lookup *lookup, struct VNode *vnode) {
  /*
    int remaining_len;
    int len;
    char *remaining;

    if (lookup->links_followed >= MAX_SYMLINKS_FOLLOWED) {
        *new_cur_vnode = NULL;
        return -EMLINK;
    }

    remaining_len = strlen(lookup.position)+1;
    remaining = lookup->buf[PATH_MAX-remaining_len];
    memmove (remaining, lookup.position, remaining_len);

    // Needs to read into kernel.
    status = vfs_readlink(vnode, lookup->buf, PATH_MAX - remaining_len);

    if (status != 0) {
        return status;
    }

    len = strlen(lookup->buf);
    memmove(lookup->buf[len], remaining, remaining_len);

    // Update lookup fields
    lookup->position = &lookup->buf[0];
    lookup->links_followed ++;

*/
  return 0;
}




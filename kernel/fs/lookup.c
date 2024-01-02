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


/* @brief Vnode lookup of a path
 *
 * @param _path
 * @param flags    0 - Lookup vnode of file or dir, do not return parent
 *                 LOOKUP_PARENT  - return parent and optionally vnode if it exists
 *                 LOOKUP_REMOVE - return parent AND vnode
 *                 LOOKUP_NOFOLLOW - Do not follow the last component if a symlink (is this in conjunction with PARENT?
 * @param lookup
 * @returns 0 on success, negative errno on error
 *
 * FIXME: Do we call vfs_close when we vnode_put an inode that has zero reference count?
 */
int lookup(char *_path, int flags, struct lookupdata *ld)
{
  int rc;
  
  if ((rc = init_lookup(_path, flags, ld)) != 0) {
    Error("Lookup init failed");
    return rc;
  }

  if (flags & LOOKUP_PARENT) {    
    if (ld->path[0] == '/' && ld->path[1] == '\0') {  // Replace with IsPathRoot()
      Error("Lookup failed root");
      return -EINVAL;  
    }

    if ((rc = lookup_path(ld)) != 0) {
      Error("Lookup failed");
      return rc;
    }

    ld->parent = ld->vnode;
    ld->vnode = NULL;
    
    rc = lookup_last_component(ld);

    // What if it failed for some other reason than it doesn't exit, could this be a vulnerability?    
    // What if it is path/. ?
    // Need to throw an error, we are looking for a path and a last component, path/. is not valid
    return 0;

  } else if (flags & LOOKUP_REMOVE) {
    Error("Lookup remove failed");
    return -ENOTSUP;
        
  } else {
    if (ld->path[0] == '/' && ld->path[1] == '\0') { // Replace with IsPathRoot()
      ld->parent = NULL;
      ld->vnode = root_vnode;
      vnode_inc_ref(ld->vnode);
      vnode_lock(ld->vnode);
      return 0;
    }

    if ((rc = lookup_path(ld)) != 0) {
      return rc;
    }
          
    ld->parent = ld->vnode;
    ld->vnode = NULL;
    
    KASSERT(ld->parent != NULL);
                
    rc = lookup_last_component(ld);

    if (ld->parent != ld->vnode)            
    {
      vnode_put(ld->parent);
    }
    
    return rc;
  }
}


/* @brief Initialize the state for performing a pathname lookup
 *
 * @param _path
 * @param flags
 * @param lookup
 * @return 0 on success, negative errno on error
 */
int init_lookup(char *_path, uint32_t flags, struct lookupdata *ld)
{
  struct Process *current;
  int path_len;
  
  current = get_current_process();
  
  ld->vnode = NULL;
  ld->parent = NULL;
  ld->position = ld->path;
  ld->last_component = NULL;
  ld->flags = flags;
  ld->path[0] = '\0';

  if (flags & LOOKUP_KERNEL) {
    StrLCpy(ld->path, _path, sizeof ld->path);
  } else if (CopyInString(ld->path, _path, sizeof ld->path) == -1) {
    return -EFAULT; // FIXME:  Could be ENAMETOOLONG 
  }

  path_len = StrLen(ld->path);

  for (size_t i = path_len; i > 0 && ld->path[i] == '/'; i--) {
    ld->path[i] = '\0';
  }
  
  ld->start_vnode = (ld->path[0] == '/') ? root_vnode : current->fproc->current_dir;    

  KASSERT(ld->start_vnode != NULL);

  if (!S_ISDIR(ld->start_vnode->mode)) {
    return -ENOTDIR;
  }

  return 0;
}


/* @brief Lookup the path to the second last component
 *
 * @param lookup - Lookup state
 * @return 0 on success, negative errno on error
 */
int lookup_path(struct lookupdata *ld)
{
  struct VNode *vnode;
  int rc;
  
  KASSERT(ld->start_vnode != NULL);

  ld->parent = NULL;
  ld->vnode = ld->start_vnode;

  vnode_inc_ref(ld->vnode);
  vnode_lock(ld->vnode);  
  
  while(1) {    
    ld->last_component = path_token(ld);
      
    if (ld->last_component == NULL) {
      rc = -EINVAL;
      break;
    }

    if (ld->parent != NULL) {
      vnode_put(ld->parent);
      ld->parent = NULL;
    }  
    
    if (is_last_component(ld)) {
      rc = 0;
      break;
    }    
                    
    ld->parent = ld->vnode;
    ld->vnode = NULL;    
    
    rc = walk_component(ld);

    if (rc != 0) {
      vnode_put(ld->parent);
      ld->parent = NULL;
      break;
    }
    
    // TODO: Check component is a directory, check permissions
  }
     
  return rc;
}


/* @brief Lookup the last component of a pathname
 *
 */
int lookup_last_component(struct lookupdata *ld)
{
  char *name;
  struct VNode *vnode;
  int rc;
 
  KASSERT(ld->parent != NULL);
    
  if (ld->last_component == NULL) {
    return -ENOENT;
  }

  if ((rc = walk_component(ld)) != 0) {
    return rc;
  }
  
  return 0;
}


/* @brief Tokenize the next pathname component, 
 *
 * @param lookup - Lookup state
 * @return - Pathname component null terminated string
 */
char *path_token(struct lookupdata *ld)
{
  char *ch;
  char *name;

  ch = ld->position;
  
  while (*ch == '/') {
    ch++;
  }

  if (*ch == '\0') {
    ld->position = ch;
    ld->separator = '\0';
    
    return NULL;
  }

  name = ch;

  while (*ch != '/' && *ch != '\0') {
    ch++;
  }

  if (*ch == '/') {
    ld->position = ch + 1;
    ld->separator = '/';
  } else {
    ld->position = ch;
    ld->separator = '\0';  
  }

  *ch = '\0';
  return name;
}


/* @brief Determine if it has reached the last pathname component
 *
 * Check only trailing / characters or null terminator after vnode component
 * 
 * PathToken to store last character replaced, either '\0 or '/'
 *
 * @param lookup - Lookup state
 * @return true if this is the last component, false otherwise
 */
bool is_last_component(struct lookupdata *ld)
{
//  char *ch;
  
  if (ld->separator == '\0') {
    return true;
  }
  
//  for (ch = ld->position; *ch == '/'; ch++) {
//  }
  
  return (*ld->position == '\0');
  
/*
  {
    return true;
  } else {
    return false;
  }
*/

}


/* @brief Walk the vnode component
 *
 * Update lock on new component, release lock on old component
 *
 * @param lookup - Lookup state
 * @param name - Filename to lookup
 * @return 
 */
int walk_component(struct lookupdata *ld)
{
  struct VNode *covered_vnode;
  struct VNode *vnode_mounted_here;
  int rc;

  KASSERT(ld != NULL);
  KASSERT(ld->parent != NULL);
  KASSERT(ld->vnode == NULL);
  KASSERT(ld->last_component != NULL);
  
  if (!S_ISDIR(ld->parent->mode)) {
    Error("ld->parent is not a directory");
    return -ENOTDIR;   
 
  } else if (StrCmp(ld->last_component, ".") == 0) {    
    
    vnode_inc_ref(ld->parent);    
    ld->vnode = ld->parent;
    return 0;
  
  } else if (StrCmp(ld->last_component, "..") == 0) {
    if (ld->parent == root_vnode) {
      vnode_inc_ref(root_vnode);
      ld->vnode = root_vnode;
      return 0;
 
    } else if (ld->parent->vnode_covered != NULL) {
      vnode_inc_ref(ld->parent->vnode_covered);
  
      vnode_put(ld->parent);
      ld->parent = ld->parent->vnode_covered;
    }
  }

  KASSERT(ld->parent != NULL);
    
  if ((rc = vfs_lookup(ld->parent, ld->last_component, &ld->vnode)) != 0) {
    return rc;
  }
  
  vnode_mounted_here = ld->vnode->vnode_mounted_here;

  // TODO: Check permissions/access here
    
  if (vnode_mounted_here != NULL) {
    vnode_put(ld->vnode);
    ld->vnode = vnode_mounted_here;
    vnode_inc_ref(ld->vnode);
    vnode_lock(ld->vnode);
  }

  return 0;
}




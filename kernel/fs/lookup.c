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

// #define KDEBUG

#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <string.h>

/* Static Prototypes
 */
static int InitLookup(char *_path, uint32_t flags, struct Lookup *lookup);
static int LookupPath(struct Lookup *lookup);
static int LookupLastComponent(struct Lookup *lookup);
static char *PathToken(struct Lookup *lookup);
static bool IsLastComponent(struct Lookup *lookup);
static int WalkComponent (struct Lookup *lookup);


/* @brief Vnode lookup of a path
 *
 * @param _path
 * @param flags    0 - Lookup vnode of file or dir, do not return parent
                   LOOKUP_PARENT  - return parent and optionally vnode if it exists
                   LOOKUP_REMOVE - return parent AND vnode
                   LOOKUP_NOFOLLOW - Do not follow the last component if a symlink (is this in conjunction with PARENT?
 * @param lookup
 * @returns 0 on success, negative errno on error
 */
int Lookup(char *_path, int flags, struct Lookup *lookup)
{
  int rc;
  
  if ((rc = InitLookup(_path, flags, lookup)) != 0) {
    return rc;
  }

  if (flags & LOOKUP_PARENT) {    
    if (lookup->path[0] == '/' && lookup->path[1] == '\0') {  // Replace with IsPathRoot()
      return -EINVAL;  
    }

    if ((rc = LookupPath(lookup)) != 0) {
      return rc;
    }

    lookup->parent = lookup->vnode;
    lookup->vnode = NULL;

    rc = LookupLastComponent(lookup);
    return 0;

  } else if (flags & LOOKUP_REMOVE) {
    return -ENOTSUP;
    
  } else {
      if (lookup->path[0] == '/' && lookup->path[1] == '\0') { // Replace with IsPathRoot()
        lookup->parent = NULL;
        lookup->vnode = root_vnode;
        VNodeIncRef(lookup->vnode);
        return 0;
      }

      if ((rc = LookupPath(lookup)) != 0) {
        return rc;
      }
            
      lookup->parent = lookup->vnode;
      lookup->vnode = NULL;
      
      KASSERT(lookup->parent != NULL);
                  
      rc = LookupLastComponent(lookup);
      
      if (rc == 0) {
        VNodePut(lookup->parent);
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
static int InitLookup(char *_path, uint32_t flags, struct Lookup *lookup)
{
  struct Process *current;
  
  current = GetCurrentProcess();
  
  lookup->vnode = NULL;
  lookup->parent = NULL;
  lookup->position = lookup->path;
  lookup->last_component = NULL;
  lookup->flags = flags;
  lookup->path[0] = '\0';

  if (flags & LOOKUP_KERNEL) {
    StrLCpy(lookup->path, _path, sizeof lookup->path);
  } else if (CopyInString(lookup->path, _path, sizeof lookup->path) == -1) {
    return -EFAULT; // FIXME:  Could be ENAMETOOLONG 
  }

  for (size_t i = StrLen(lookup->path); i > 0 && lookup->path[i] == '/'; i--) {
    lookup->path[i] = '\0';
  }
  
  lookup->start_vnode = (lookup->path[0] == '/') ? root_vnode : current->current_dir;    

  KASSERT(lookup->start_vnode != NULL);

  if (!S_ISDIR(lookup->start_vnode->mode)) {
    Info ("root vnode is not a directory");
    return -ENOTDIR;
  }

  return 0;
}


/* @brief Lookup the path to the second last component
 *
 * @param lookup - Lookup state
 * @return 0 on success, negative errno on error
 */
static int LookupPath(struct Lookup *lookup)
{
  struct VNode *vnode;
  int rc;
  
  KASSERT(lookup->start_vnode != NULL);

  lookup->parent = NULL;
  lookup->vnode = lookup->start_vnode;

  
//  VNodeLock(dvnode);
  
  for(;;) {    
    lookup->last_component = PathToken(lookup);
      
    if (lookup->last_component == NULL) {
      return -EINVAL;
    }
    
    if (IsLastComponent(lookup)) {
      return 0; 
    }    

    if (lookup->parent != NULL) {
      VNodePut(lookup->parent);
      lookup->parent = NULL;
    }  
                    
    lookup->parent = lookup->vnode;
    lookup->vnode = NULL;    
    
    rc = WalkComponent(lookup);

    if (rc != 0)
    {
      VNodePut(lookup->parent);
      lookup->parent = NULL;
      return rc;
    }
    
    // Check component is a directory, check permissions
  }    

  Info ("**** LookupPath shouldn't get here ****");
  return -ENOSYS;
}


/* @brief Lookup the last component of a pathname
 *
 */
static int LookupLastComponent(struct Lookup *lookup)
{
  char *name;
  struct VNode *vnode;
  int rc;
 
  KASSERT(lookup->parent != NULL);
    
  if (lookup->last_component == NULL) {
    return -ENOENT;
  }

  if ((rc = WalkComponent(lookup)) != 0) {
    return rc;
  }
  
  return 0;
}


/* @brief Tokenize the next pathname component, 
 *
 * @param lookup - Lookup state
 * @return - Pathname component null terminated string
 */
static char *PathToken(struct Lookup *lookup)
{
  char *ch;
  char *name;

  ch = lookup->position;
  
  while (*ch == '/') {
    ch++;
  }

  if (*ch == '\0') {
    lookup->position = ch;
    lookup->separator = '\0';
    
    return NULL;
  }

  name = ch;

  while (*ch != '/' && *ch != '\0') {
    ch++;
  }

  if (*ch == '/')
  {
    lookup->position = ch + 1;
    lookup->separator = '/';
  }
  else
  {
    lookup->position = ch;
    lookup->separator = '\0';  
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
static bool IsLastComponent(struct Lookup *lookup)
{
//  char *ch;
  
  if (lookup->separator == '\0') {
    return true;
  }
  
//  for (ch = lookup->position; *ch == '/'; ch++) {
//  }
  
  return (*lookup->position == '\0');
  
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
static int WalkComponent (struct Lookup *lookup)
{
  struct VNode *covered_vnode;
  struct VNode *vnode_mounted_here;
  int rc;

  KASSERT(lookup != NULL);
  KASSERT(lookup->parent != NULL);
  KASSERT(lookup->vnode == NULL);
  KASSERT(lookup->last_component != NULL);
  
  if (!S_ISDIR(lookup->parent->mode)) {
    Info ("lookup->parent is not a directory");
    return -ENOTDIR;   
 
  } else if (StrCmp(lookup->last_component, ".") == 0) {    
    VNodeIncRef(lookup->parent);    
    lookup->vnode = lookup->parent;
    return 0;
  
  } else if (StrCmp(lookup->last_component, "..") == 0) {
    if (lookup->parent == root_vnode) {
      VNodeIncRef(root_vnode);
      lookup->vnode = root_vnode;
      return 0;
 
    } else if (lookup->parent->vnode_covered != NULL) {
      VNodeIncRef(lookup->parent->vnode_covered);
      VNodePut(lookup->parent);
      lookup->parent = lookup->parent->vnode_covered;
    }
  }

  KASSERT(lookup->parent != NULL);
    
  if ((rc = vfs_lookup(lookup->parent, lookup->last_component, &lookup->vnode)) != 0) {
    return rc;
  }
  
  vnode_mounted_here = lookup->vnode->vnode_mounted_here;

  // TODO: Check permissions/access here
    
  if (vnode_mounted_here != NULL) {
    VNodePut(lookup->vnode);
    lookup->vnode = vnode_mounted_here;
    VNodeIncRef(lookup->vnode);
  }

  return 0;
}




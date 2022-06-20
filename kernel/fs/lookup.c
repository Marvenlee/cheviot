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

/* Static Prototypes
 */
static int init_lookup(char *_path, uint32_t flags, struct lookupdata *ld);
static int lookup_path(struct lookupdata *ld);
static int lookup_last_component(struct lookupdata *ld);
static char *path_token(struct lookupdata *ld);
static bool is_last_component(struct lookupdata *ld);
static int walk_component (struct lookupdata *ld);


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
int lookup(char *_path, int flags, struct lookupdata *ld)
{
  int rc;
  
  if ((rc = init_lookup(_path, flags, ld)) != 0) {
    Info ("Lookup init failed");
    return rc;
  }

  if (flags & LOOKUP_PARENT) {    
    Info("---");
    Info("Lookup parent");
    
    if (ld->path[0] == '/' && ld->path[1] == '\0') {  // Replace with IsPathRoot()
      Info ("Lookup failed root");
      return -EINVAL;  
    }

    if ((rc = lookup_path(ld)) != 0) {
      Info ("Lookup failed x");
      return rc;
    }

    ld->parent = ld->vnode;
    ld->vnode = NULL;
    
    Info ("ld.parent = %08x", ld->parent);

    rc = lookup_last_component(ld);

    Info ("Lookup last component rc=%d", rc);
    
    // What if it failed for some other reason than it doesn't exit, could this be a vulnerability?    

    // What if it is path/.
    // Need to throw an error, we are looking for a path and a last component, path/. is not valid

    return 0;

  } else if (flags & LOOKUP_REMOVE) {
    Info("Lookup remove failed");
    return -ENOTSUP;
    
  } else {
      if (ld->path[0] == '/' && ld->path[1] == '\0') { // Replace with IsPathRoot()
        Info("Lookup locking root");
        ld->parent = NULL;
        ld->vnode = root_vnode;
        vnode_inc_ref(ld->vnode);
        vnode_lock(ld->vnode);
        return 0;
      }

      if ((rc = lookup_path(ld)) != 0) {
        Info ("Lookup failed");
        return rc;
      }
            
      ld->parent = ld->vnode;
      ld->vnode = NULL;
      
      KASSERT(ld->parent != NULL);
                  
      rc = lookup_last_component(ld);

      if (ld->parent != ld->vnode)            
      {
        Info ("Lookup . parent == vnode = %08x", ld->parent);
        vnode_put(ld->parent);
      }
      
      Info ("Lookup ret=%d", rc);      
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
static int init_lookup(char *_path, uint32_t flags, struct lookupdata *ld)
{
  struct Process *current;
  
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

  for (size_t i = StrLen(ld->path); i > 0 && ld->path[i] == '/'; i--) {
    ld->path[i] = '\0';
  }
  
  Info ("Lookup patn:%s", ld->path);
  
  ld->start_vnode = (ld->path[0] == '/') ? root_vnode : current->current_dir;    

  KASSERT(ld->start_vnode != NULL);

  if (!S_ISDIR(ld->start_vnode->mode)) {
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
static int lookup_path(struct lookupdata *ld)
{
  struct VNode *vnode;
  int rc;
  
  KASSERT(ld->start_vnode != NULL);

  Info("LookupPath()");

  ld->parent = NULL;
  ld->vnode = ld->start_vnode;

  vnode_inc_ref(ld->vnode);
  vnode_lock(ld->vnode);  
  
  for(;;) {    
    ld->last_component = path_token(ld);
      
    if (ld->last_component == NULL) {
      return -EINVAL;
    }

    if (ld->parent != NULL) {
      Info("lookuppath vnodeput 1");
      vnode_put(ld->parent);
      ld->parent = NULL;
    }  
    
    if (is_last_component(ld)) {
      return 0;
    }    
                    
    ld->parent = ld->vnode;
    ld->vnode = NULL;    
    
    rc = walk_component(ld);

    if (rc != 0)
    {
      Info("lookuppath vnodeput 2");
      vnode_put(ld->parent);
      ld->parent = NULL;
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
static int lookup_last_component(struct lookupdata *ld)
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
static char *path_token(struct lookupdata *ld)
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

  if (*ch == '/')
  {
    ld->position = ch + 1;
    ld->separator = '/';
  }
  else
  {
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
static bool is_last_component(struct lookupdata *ld)
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
static int walk_component(struct lookupdata *ld)
{
  struct VNode *covered_vnode;
  struct VNode *vnode_mounted_here;
  int rc;

  KASSERT(ld != NULL);
  KASSERT(ld->parent != NULL);
  KASSERT(ld->vnode == NULL);
  KASSERT(ld->last_component != NULL);
  
  Info("WalkComponent path:%s", ld->last_component);
  
  if (!S_ISDIR(ld->parent->mode)) {
    Info ("ld->parent is not a directory");
    return -ENOTDIR;   
 
  } else if (StrCmp(ld->last_component, ".") == 0) {    
    
    Info("Walk . ld->parent = %08x", ld->parent);    
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
  
      Info("walkcomponent vnodeput 1");
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
      Info("walkcomponent vnodeput 2");
    vnode_put(ld->vnode);
    ld->vnode = vnode_mounted_here;
    vnode_inc_ref(ld->vnode);
    vnode_lock(ld->vnode);
  }

  return 0;
}




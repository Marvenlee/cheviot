#include <sys/syscalls.h>
#include <sys/lists.h>
#include <sys/debug.h>
#include <interfaces/block.h>
#include <interfaces/filesystem.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "filesystem.h"




/*
 * Lookup();
 *
 * Commands
 * LOOKUP   - returns the vnode of the actual file looked up
 * LASTDIR  - returns directory vnode and last component of pathname
 *
 * Flags
 * FOLLOW   - follow a symlink
 * NOFOLLOW - do not follow a symlink
 *
 * FIXME: Tidy up.
 */

struct VNode *LookupLastDir (struct VNode *dvnode, char *pathname, class FileDesc *fd)
{
	struct VNode *cur_vnode;
	struct VNode *new_vnode;
	char *remaining;
	char *next;
	struct Mount *mount;


	*r_last_component = '\0';
	
	
	cur_vnode = dvnode;
	remaining = path;
		
	if (cur_vnode == NULL)
	{
		KLog ("cur_vnode == NULL");
	
		fd->error = EPERM;
		return NULL;
	}
	
	VNodeHold (cur_vnode);
	
	
	
	while (1)
	{
		next = GetName (remaining, r_last_component);
							
		if (*next == '\0')
		{
			if (cmd == LASTDIR)
			{
				if (cur_vnode->type == VNODE_TYPE_DIR)
				{
					return cur_vnode;
				}
				else
				{
					VNodeRelease (cur_vnode);
					fd->error = ENOTDIR;			
					return NULL;
				}
			}
			else if (r_last_component[0] == '\0')
			{
				return cur_vnode;
			}
		}
		

		new_vnode = Advance (cur_vnode, r_last_component, fd);
				
		if (new_vnode == NULL)
		{
			VNodeRelease (cur_vnode);
			return NULL;
		}
		else if (new_vnode->type == VNODE_TYPE_DIR || new_vnode->type == VNODE_TYPE_REG)
		{
			VNodeRelease (cur_vnode);
			remaining = next;
			cur_vnode = new_vnode;
		}
		else if (new_vnode->type == VNODE_TYPE_MOUNT)
		{
			mount = (struct Mount *)new_vnode->data;

			VNodeRelease (new_vnode);
			VNodeRelease (cur_vnode);
			
			cur_vnode = mount->sb->root;
			
			VNodeHold (cur_vnode);
			remaining = next;
		}
		else
		{
			fd->error = EIO;
			VNodeRelease (cur_vnode);
			VNodeRelease (new_vnode);
			return NULL;
		}
	}
}





struct VNode *LookupFile (struct VNode *dvnode, char *filename, class FileDesc *fd)
{
	struct VNode *cur_vnode;
	struct VNode *new_vnode;
	char *remaining;
	char *next;
	struct Mount *mount;

//	*r_last_component = '\0';
	
	
	cur_vnode = dvnode;
	remaining = path;
		
	if (cur_vnode == NULL)
	{
		KLog ("cur_vnode == NULL");
	
		fd->error = EPERM;
		return NULL;
	}
	
	VNodeHold (cur_vnode);
	
	
	
	while (1)
	{
		next = GetName (remaining, r_last_component);
							
		if (*next == '\0')
		{
			if (cmd == LASTDIR)
			{
				if (cur_vnode->type == VNODE_TYPE_DIR)
				{
					return cur_vnode;
				}
				else
				{
					VNodeRelease (cur_vnode);
					fd->error = ENOTDIR;			
					return NULL;
				}
			}
			else if (r_last_component[0] == '\0')
			{
				return cur_vnode;
			}
		}
		

		new_vnode = Advance (cur_vnode, r_last_component, fd);
				
		if (new_vnode == NULL)
		{
			VNodeRelease (cur_vnode);
			return NULL;
		}
		else if (new_vnode->type == VNODE_TYPE_DIR || new_vnode->type == VNODE_TYPE_REG)
		{
			VNodeRelease (cur_vnode);
			remaining = next;
			cur_vnode = new_vnode;
		}
		else if (new_vnode->type == VNODE_TYPE_MOUNT)
		{
			mount = (struct Mount *)new_vnode->data;

			VNodeRelease (new_vnode);
			VNodeRelease (cur_vnode);
			
			cur_vnode = mount->sb->root;
			
			VNodeHold (cur_vnode);
			remaining = next;
		}
		else
		{
			fd->error = EIO;
			VNodeRelease (cur_vnode);
			VNodeRelease (new_vnode);
			return NULL;
		}
	}
}




















/*
 * Copy the next component from the remaining pathname.
 * Returns a pointer to the component after the next component.
 */
 
char *GetName (char *remaining, char *component)
{
	char *cp, *rp;

	cp = component;
	rp = remaining;
	
	while ( *rp == '/')
  		rp++;
  
	while (*rp != '/' && *rp != '\0')
	{
		*cp++ = *rp++;
	}
	
	*cp = '\0';
		
	while (*rp == '/')
		rp++;

	return rp;
}




/*
 * Looks up the component in the current vnode's directory. Return the vnode of the
 * component we lookup.
 *
 * AWOOGA: FIXME: CHECKME:  Why are vnode hold locks commented out here?
 */

struct VNode *Advance (struct VNode *cur_vnode, char *component, class FileDesc *fd)
{
	struct VNode *new_vnode;
	
	if (cur_vnode == NULL)
		return NULL;
	
	if (component[0] == '\0')
		return cur_vnode;

	if (fd->IsAllowed(cur_vnode, X_BIT) == -1)
		return NULL;
	
	
	// Why not just add . and .. entries into root directory?
	if (cur_vnode == cur_vnode->superblock->root)
	{
		if (strcmp ("..", component) == 0)
			return &root_vnode;
		else if (strcmp (".", component) == 0)
			return cur_vnode;
	}
		
	if ((DNameLookup (cur_vnode, component, &new_vnode)) == 0 && new_vnode == NULL)
		return NULL;
	
	if (cur_vnode->vfs->lookup(cur_vnode, component, &new_vnode, fd) != 0)
	{
		DNameEnter (cur_vnode, NULL, component);
		return NULL;
	}
	
	DNameEnter (cur_vnode, new_vnode, component);
	return new_vnode;
	
}




/*
 * Looks up a vnode in the DNLC based on parent directory vnode
 * and the filename. Stores the resulting vnode pointer in vnp.
 */
 
int DNameLookup (struct VNode *dir, char *name, struct VNode **vnp)
{
	int t;
	int sz;
	int key;
	struct DName *dname;
	

	sz = strlen (name) + 1;
	
	if (sz > DNAME_SZ)
	{
		*vnp = NULL;
		return -1;
	}
		
	for (key=0, t=0; t < sz && name[t] != '\0'; t++)
		key += name[t];
	
	key %= DNAME_HASH;


	dname = LIST_HEAD (&dname_hash[key]);
	
	while (dname != NULL)
	{
		if (dname->dir_vnode == dir && strcmp (dname->name, name) == 0)
		{
			*vnp = dname->vnode;
			return 0;
		}
			
		dname = LIST_NEXT (dname, hash_link);
	}

	*vnp = NULL;
	return -1;
}




/*
 * Add an entry to the DNLC
 */

int DNameEnter (struct VNode *dir, struct VNode *vn, char *name)
{
	int t;
	int sz;
	int key;
	struct DName *dname;
	
	sz = strlen (name) + 1;
	
	if (sz > DNAME_SZ)
		return -1;
	
	for (key=0, t=0; t < sz && name[t] != '\0'; t++)
		key += name[t];
	
	key %= DNAME_HASH;

	dname = LIST_HEAD (&dname_hash[key]);
	
	while (dname != NULL)
	{
		if (dname->dir_vnode == dir && strcmp (dname->name, name) == 0)
		{
			dname->hash_key = key;
			dname->dir_vnode = dir;
			dname->vnode = vn;
			return 0;
		}
			
		dname = LIST_NEXT (dname, hash_link);
	}

	dname = LIST_HEAD (&dname_lru_list);

	LIST_REM_HEAD (&dname_lru_list, lru_link);

	if (dname->hash_key != -1)
	{
		LIST_REM_ENTRY (&dname_hash[dname->hash_key], dname, hash_link);
	}
	
	dname->hash_key = key;
	dname->dir_vnode = dir;
	dname->vnode = vn;
	strlcpy (dname->name, name, DNAME_SZ);
	
	LIST_ADD_TAIL (&dname_lru_list, dname, lru_link);
	LIST_ADD_HEAD (&dname_hash[key], dname, hash_link);
	return 0;
}




/*
 * Remove a single DNLC entry using the parent directory vnode and filename
 */
 
int DNameRemove (struct VNode *dir, char *name)
{
	int t;
	int sz;
	int key;
	struct DName *dname;
	
	KLog ("DNameRemove(%s)", name);
	
	sz = strlen (name) + 1;
	
	if (sz > DNAME_SZ)
		return -1;
		
	for (key=0, t=0; t < sz && name[t] != '\0'; t++)
		key += name[t];
		
	key %= DNAME_HASH;
	dname = LIST_HEAD (&dname_hash[key]);
	
	while (dname != NULL)
	{
		if (strcmp (dname->name, name) == 0)
		{
			LIST_REM_ENTRY (&dname_hash[key], dname, hash_link);
			dname->hash_key = -1;
			LIST_REM_ENTRY (&dname_lru_list, dname, lru_link);
			LIST_ADD_HEAD (&dname_lru_list, dname, lru_link);
			return 0;
		}
		
		dname = LIST_NEXT (dname, hash_link);
	}
			
	return -1;
}



/*
 * Removes any DNLC entry associated with a vnode, whether it is
 * a directory_vnode or the vnode it points to,
 */

int DNamePurgeVNode (struct VNode *vnode)
{
	int t;

	KLog ("DNamePurgeVNode()");
	
	for (t=0; t< NR_DNAME; t++)
	{
		if (dname_table[t].hash_key != -1
			&& (dname_table[t].vnode == vnode || dname_table[t].dir_vnode == vnode))
		{
			LIST_REM_ENTRY (&dname_hash[dname_table[t].hash_key], &dname_table[t], hash_link);
			dname_table[t].hash_key = -1;
			LIST_REM_ENTRY (&dname_lru_list, &dname_table[t], lru_link);
			LIST_ADD_HEAD (&dname_lru_list, &dname_table[t], lru_link);
		}
	}
	
	return 0;
}




/*
 * Removes all DNLC entries associated with VNodes belonging to the
 * SuperBlock.
 */

int DNamePurgeSuperblock (struct SuperBlock *sb)
{
	int t;

	KLog ("DNamePurgeSuperblock()");
	
	for (t=0; t< NR_DNAME; t++)
	{
		if (dname_table[t].hash_key != -1
			&& (dname_table[t].vnode->superblock == sb))
		{
			LIST_REM_ENTRY (&dname_hash[dname_table[t].hash_key], &dname_table[t], hash_link);
			dname_table[t].hash_key = -1;
			LIST_REM_ENTRY (&dname_lru_list, &dname_table[t], lru_link);
			LIST_ADD_HEAD (&dname_lru_list, &dname_table[t], lru_link);
		}
	}

	return 0;
}




#include <sys/syscalls.h>
#include <sys/lists.h>
#include <sys/debug.h>
#include <interfaces/block.h>
#include <interfaces/filesystem.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "filesystem.h"




/*
 * Finds a vnode from a given superblock and vnode number.
 * Returns an existing vnode if it is still in the vnode cache.
 * If the vnode is not in the cache then another free vnode is
 * chosen. Data pointed to by the vnode->data field is freed first
 * before returning the vnode.
 *
 * returns a vnode structure, if not existing already then V_VALID
 * is cleared in the flags field.
 */

struct VNode *VNodeGet (struct SuperBlock *sb, int inode_nr)
{
	struct VNode *vnode;
	
	KAssert (sb != NULL);			

	while (1)
	{
		if (sb->flags & S_ABORT)
			return NULL;
		
		if ((vnode = FindVNode(sb, inode_nr)) != NULL)
		{
			if (vnode->busy == 1)
			{
				tasksleep(&vnode->rendez);
				continue;
			}
			
			if ((vnode->flags & V_FREE) == V_FREE)
			{
				LIST_REM_ENTRY(&vnode_free_list, vnode, vnode_entry);
			}
			
			vnode->busy = 1;
			vnode->flags = 0;
			vnode->reference_cnt++;
			sb->reference_cnt++;
			vnode->inode_nr = inode_nr;
			return vnode;			
		}
		else
		{
			vnode = LIST_HEAD(&vnode_free_list);
			
			if (vnode == NULL)
				return NULL;
			
			LIST_REM_HEAD (&vnode_free_list, vnode_entry);

			if (vnode->data != NULL)
				free (vnode->data);
			
			DNamePurgeVNode(vnode);
			
			vnode->data = NULL;
			vnode->busy = 1;
			vnode->flags = 0;
			vnode->superblock = sb;
			vnode->inode_nr = inode_nr;
			vnode->reference_cnt = 1;
			sb->reference_cnt++;
						
			return vnode;
		}
	}
}




/*
 * VNodeHold();
 */

void VNodeHold (struct VNode *vnode)
{
	vnode->reference_cnt++;
	vnode->superblock->reference_cnt++;
}





/*
 *
 */
 
void VNodeRelease (struct VNode *vnode)
{
	struct SuperBlock *sb;
	
	KAssert (vnode != NULL);
	KAssert (vnode->superblock != NULL);
	
	vnode->reference_cnt--;
	vnode->superblock->reference_cnt --;
	
	if (vnode->reference_cnt == 0)
	{
		sb  = vnode->superblock;

		vnode->vfs->close(vnode);			// FIXME: should we be closing in VNodeRelease()??
		                                    // Also is it called on each close() call?
		                                    // What about deleting a file that is marked
		                                    // for deletion and reference_cnt is finally zero?
		                                    
		
		if ((vnode->flags & V_ROOT) == 0)
		{
			vnode->flags |= V_FREE;
			LIST_ADD_TAIL(&vnode_free_list, vnode, vnode_entry);
		}
		
		if (sb->reference_cnt == 0 && sb->flags & S_ABORT)
			taskwakeup (&sb->rendez);
	}
	
	vnode->busy = 0;
	taskwakeupall (&vnode->rendez);
}




/*
 *
 */
 
void VNodeLock (struct VNode *vnode)
{
	while (vnode->busy == 1)
		tasksleep (&vnode->rendez);
		
	vnode->busy = 1;
}




/*
 * VNodeUnlock();
 */

void VNodeUnlock (struct VNode *vnode)
{
	vnode->busy = 0;
	taskwakeupall (&vnode->rendez);
}







/*
 * FindVNode();
 * FIXME: Hash vnode by inode_nr
 */

struct VNode *FindVNode (struct SuperBlock *sb, int inode_nr)
{
	int v;
	
	
	for (v=0; v< NR_VNODE; v++)
	{
		if (/* FIXME:????? (vnode_table[v].flags & V_FREE) != V_FREE &&*/
			vnode_table[v].superblock == sb &&
			vnode_table[v].inode_nr == inode_nr)
		{
			return &vnode_table[v];
		}
	}
		
	return NULL;
}




/*
 * AllocVNode();
 */

struct VNode *AllocVNode(void)
{
	struct VNode *vnode;
	
	
	vnode = LIST_HEAD(&vnode_free_list);

	if (vnode == NULL)
		return NULL;
	
	LIST_REM_HEAD (&vnode_free_list, vnode_entry);
	vnode->flags = 0;
	vnode->reference_cnt = 1;
	
	return vnode;
}



void FreeVNode (struct VNode *vnode)
{
	vnode->flags = V_FREE;
	LIST_ADD_HEAD(&vnode_free_list, vnode, vnode_entry);

	vnode->busy = 0;
	vnode->reference_cnt = 0;
	taskwakeupall (&vnode->rendez);
}

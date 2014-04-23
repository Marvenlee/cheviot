#include <sys/types.h>
#include <sys/syscalls.h>
#include <interfaces/filesystem.h>
#include <sys/debug.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include "filesystem.h"
#include <new> 

enum 
{   
    notExistErr  = -20,
    notAFileErr  = -21,
    notADirErr   = -22,
    accessErr    = -23
};





/*
 *  Task for an open file descriptor / filp
 */

void FileDesc::FileDescTask (void *arg)
{
    class FileDesc *fd = (class FileDesc *)arg;
    size_t msg_sz;
    struct FileMsg *msg;
    int exit_status = 0;
    

    try
    {        
        while (exit_status == 0)
        {
            msg_sz = GetMsg (fd->handle, (void **)&msg, PROT_READWRITE);
    
            if (msg_sz >= (ssize_t) sizeof *msg)
            {
                switch (msg->cmd)
    			{	
    				case EXEC_CMD_OPEN:
    					fd->open();
    					break;
    					
    				case EXEC_CMD_CLOSE:
    					fd->close();
    					break;
    					
    				case EXEC_CMD_LSEEK:
    					fd->lseek();
    					break;
    					
    				case EXEC_CMD_READ:
    					fd->read();
    					break;
    					
    				case EXEC_CMD_WRITE:
    					fd->write();
    					break;
    								
    				case EXEC_CMD_OPENDIR:
    					fd->opendir();
    					break;
    				
    				case EXEC_CMD_READDIR:
    					fd->readdir();
    					break;
    					
    				case EXEC_CMD_REWINDDIR:
    					fd->rewinddir();
    					break;	
    												
    				case EXEC_CMD_MKDIR:
    					fd->mkdir();
    					break;
    				
    				case EXEC_CMD_RMDIR:
    					fd->rmdir();
    					break;
    					
    				case EXEC_CMD_REMOVE:
    					fd->remove();
    					break;
    				
    				case EXEC_CMD_RENAME:
    					fd->rename();
    					break;
    					
    				case EXEC_CMD_FSTAT:
    					fd->fstat();
    					break;
    				
    				case EXEC_CMD_STAT:
    					fd->stat();
    					break;
    					
    				case EXEC_CMD_FTRUNCATE:
    					fd->ftruncate();
    					break;
    				
    				case EXEC_CMD_TRUNCATE:
    					fd->truncate();
    					break;	
    				
    				case EXEC_CMD_CHMOD:
    					fd->chmod();
    					break;
    				
    				case EXEC_CMD_CHOWN:
    					fd->chown();
    					break;
    				
    				case EXEC_CMD_DUP:
    					fd->dup();
    					break;
    					
    				case EXEC_CMD_STARTNOTIFY:
    					fd->startnotify();
    					break;
    				
    				case EXEC_CMD_ENDNOTIFY:
    					fd->endnotify();
    					break;
    				
    				case EXEC_CMD_WAITNOTIFY:
    					fd->waitnotify();
    					break;
    									
    				default:
    					throw messageErr;
    					break;
    			}
    		}
    		else if (msg_sz == 0 && exit_status == 0)
            {
                tasksleep (&fd->rendez);
            }
            else
            {
                throw messageErr;
            }
        }
    }
    catch (int err)
    {
        fd->~FileDesc();
    }
        
    
}



FileDesc::FileDesc()
{
    int new_fd[2];
    
    if (CreateChannel(new_fd) < 0)
        throw memoryErr;
    
    if ((task_id = taskcreate (&FileDescTask, this, 2048)) == -1)
        throw memoryErr;

    fd[0] = new_fd[0];
    fd[1] = new_fd[0];
}




/* 
 *
 */

FileDesc::~FileDesc()
{
    CloseHandle (fd[1]);
    
    if (msg != NULL)
        VirtualFree (msg);
        
    VNodeRelease (vnode);
}




/*
 *
 */
 
int FileDesc::IsAllowed (struct VNode *vnode, mode_t desired_access)
{
	mode_t perm_bits;
	int shift;
	
 
 	if (this->uid == vnode->uid)
		shift = 6;					/* owner */
	else if (this->gid == vnode->gid)
		shift = 3;					/* group */
	else
		shift = 0;					/* other */
	
	perm_bits = (vnode->mode >> shift) & (R_BIT | W_BIT | X_BIT);
    
	if ((perm_bits | desired_access) != perm_bits)
	{
		this->error = accessErr;
		return -1;
	}	

	return 0;
}



/*
 *
 */

void FileDesc::open()
{
	struct VNode *dvnode;
	struct VNode *new_vnode;
	class FileDesc *new_filedesc;
	
	char component[PATH_MAX];           // Replace with pointer to last component in msg
	char *pathname, *dirname, *filename;
	mode_t mode;
	int oflags;
	struct VAttr va;

    
	if (vnode->type != VNODE_TYPE_DIR)
	{
		msg->error = notADirErr;
		if (PutMsg (h, msg, 0) < 0)
		    throw messageErr;

		return;
	}
	
	mode = msg->s.open.mode;
	oflags = msg->s.open.oflags;
	msg->s.open.pathname[MAX_PATHNAME_SZ-1] = '\0';
	pathname = msg->s.open.pathname;
    new_vnode = NULL;
    
    dirname =;
    filename =;


    if (dvnode = LookupDir (vnode, dirname, this)) == NULL)
    {
 		msg->error = error;

    	if (PutMsg (h, msg, 0) < 0)
	        throw messageErr;
    }
        
	
	if (new_vnode = LookupFile (dvnode, filename, this) == NULL)
	{
	    if (flags & O_CREAT && IsAllowed (dvnode, W_BIT) == 0)
	    {
      		va.mode = mode;
    		va.uid = uid;
    		va.gid = gid;
    		va.atime = time (NULL);
    		va.mtime = va.atime;
    		va.ctime = va.atime;
    		
    		dvnode->vfs->create (dvnode, component, &va, oflags, mode, &new_vnode, this);
    		
    		if (new_vnode == NULL)
    		{
    		    VNodeRelease (dvnode);
    	 		msg->error = error;
    
    	    	if (PutMsg (h, msg, 0) < 0)
    		        throw messageErr;
    
        		return;
    		}
    		
    		DNameEnter (dvnode, new_vnode, component);
    		VNodeRelease (dvnode);
        }
        else if (flags & O_CREAT == 0)
        {
        }
    }
    else
    {
		msg->error = notExistErr;

		if (PutMsg (h, msg, 0) < 0)
		    throw messageErr;

		return;
	}
	
	
	if (new_vnode->type == VNODE_TYPE_DIR)
	{
		VNodeRelease (new_vnode);
		msg->error = notAFileErr;
		
		if (PutMsg (h, msg, 0) < 0)
		    throw messageErr;

		return;
	}
	else if (msg->s.open.oflags & O_TRUNC
		&& new_vnode->vfs->truncate(new_vnode, 0, this) != 0)
	{
		VNodeRelease (new_vnode);
		msg->error = error;

		if (PutMsg (h, msg, 0) < 0)
		    throw messageErr;
		return;
	}
	

    new_filedesc = new (std::nothrow) FileDesc();

    if (new_filedesc == NULL)
    {
    	VNodeRelease (new_vnode);
		msg->error = memoryErr;
		return;
    }


	if (msg->s.open.oflags & O_APPEND)
		new_filedesc->offset = new_vnode->size;
	else
		new_filedesc->offset = 0;


    if (PutMsg (h, msg, 0) < 0)
    {
        CloseHandle (new_filedesc[0]);
        throw messageErr;
    }
        
   	if (PutHandle (h, new_filedesc->fd[0], 0) < 0)
   	{
        CloseHandle (new_filedesc[0]);
   	    throw messageErr;
   	}
   	
	VNodeUnlock (new_vnode);
	msg->error = 0;
}




/*
 *
 */
 
void FileDesc::close()
{
	// FIXME: Only update atime on close and open?


	VNodeRelease (vnode);	
	vnode = NULL;

	msg->error = 0;
	msg->r.close = 0;
		
    if (PutMsg (h, msg, 0) < 0)
        throw messageErr;
}




/*
 *
 */
 
void FileDesc::lseek()
{
	if (vnode->type != VNODE_TYPE_REG)
	{
		msg->error = paramErr;
		msg->r.lseek = -1;
	}
    else if (msg->s.lseek.whence == SEEK_SET)
	{
		offset = msg->s.lseek.offset;
		msg->r.lseek = offset;
		msg->error = 0;
		
	}
	else if (msg->s.lseek.whence == SEEK_CUR)
	{
		offset += msg->s.lseek.offset;
		msg->r.lseek = offset;
		msg->error = 0;
	}
	else if (msg->s.lseek.whence == SEEK_END)
	{
		offset = vnode->size + msg->s.lseek.offset;
		msg->r.lseek = offset;
		msg->error = 0;
	}
	else
	{
		msg->r.lseek = -1;
		msg->error = paramErr;
	}
	
    if (PutMsg (h, msg, 0) < 0)
        throw messageErr;
}




/*
 *
 */

void FileDesc::read()
{
}






void FileDesc::write()
{
}

void FileDesc::opendir()
{
}

void FileDesc::readdir()
{
}

void FileDesc::rewinddir()
{
}

void FileDesc::mkdir()
{
}

void FileDesc::rmdir()
{
}

void FileDesc::remove()
{
}

void FileDesc::rename()
{
}

void FileDesc::fstat()
{
}

void FileDesc::stat()
{
}

void FileDesc::ftruncate()
{
}


void FileDesc::truncate()
{
}

void FileDesc::chmod()
{
}

void FileDesc::chown()
{
}

void FileDesc::dup()
{
}

void FileDesc::startnotify()
{
}

void FileDesc::endnotify()
{
}

void FileDesc::waitnotify()
{
}




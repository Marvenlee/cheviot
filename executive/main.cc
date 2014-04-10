#include <sys/types.h>
#include <sys/syscalls.h>
#include <interfaces/filesystem.h>
#include "filesystem.h"




/*
 *
 */

void taskmain (int argc, char *argv[])
{
    int h;
    int exit_status = 0;
    
    
    // Initialize Executive,  create any daemon-like coroutine tasks, load drivers etc.
    // AddEventRendez (handle_xyz, &rendez);  for each handle.
        
	while (exit_status == 0)
	{	
		h = WaitFor(-1);
		
		if (h >= 0)
		{
            // WakeupEventRendez (h);

		    // Wakes up approriate task for this handle 
    	}
		
		while (taskyield() != 0);
    }
    
    // DeleteEventRendez (handle_xyz);
}





/*
 *  Task for an open file descriptor / filp
 */

void FileDesc::FileDescTask (void *arg)
{
    class FileDesc *fd = (class FileDesc *)arg;
    size_t msg_sz;
    struct FileMsg *msg;
    int exit_status = 0;
    int err;
    
    while (exit_status == 0)
    {
        msg_sz = GetMsg (fd->handle, (void **)&msg, PROT_READWRITE);
    
        if (msg_sz == 0 && exit_status == 0)
        {
            tasksleep (&fd->rendez);
            continue;
        }
        else if (err < 0 || msg_sz < (ssize_t) sizeof *msg)
        {
            exit_status = 1;
        }
        else
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
					exit_status = 1;
					break;
			}
			
			if (PutMsg (fd->handle, msg, 0) < 0)
			    exit_status = 1;
		}
    }
    
    
    // Free any messages or memory
    // close this channel
    // free filedesc,
    // decrement vnode reference count.
}





void FileDesc::open()
{
}

void FileDesc::close()
{
}


void FileDesc::lseek()
{
}

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




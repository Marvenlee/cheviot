#include <sys/syscalls.h>




/*
 *
 */

void taskmain (int argc, char *argv[])
{
    // AddEventHandler(handle_xyz, &rendez);

	while (!(exit_status == 1 && connection_cnt == 0))
	{	
		h = WaitFor(-1);
		
		if (h >= 0)
		{
            // RaiseEventHandler (h);

		    // Wakes up approriate rendez by scanning
		    // EventHandler lists to find rendez to wakeup.
    	}
		
		while (taskyield() != 0);
    }
    
    // DeleteEventHandler (handle_xyz);
}




/*
 * System task for a process
 */
 
void task_system (void *arg)
{
    struct Process *ps = arg;

    while (ps->exit_status == 0)
    {
        while (fd->msg_sz = GetMsg (fd->handle, &fd->msg, PROT_READWRITE) > 0)
        {
            switch (tf->msg->cmd)
			{	
                // EXEC_CMD_SPAWN - actually might be done on task_file 
                // EXEC_CMD_EXIT
                // EXEC_CMD_JOIN
                // EXEC_CMD_KILL
                //
                // EXEC_SETUID
                // EXEC_GETUID
                // EXEC_SETGID
                // EXEC_GETGID
                //
                // EXEC_CMD_CREATEPORT
                // EXEC_CMD_LISTEN
                // EXEC_CMD_CONNECT
            }
            
			
			iov[0] = fd->msg;
			PutMsg (fd->handle, iov, 1);
		}
		
        while (fd->msg_sz == 0 && fd->exit_status != 0)
            tasksleep (&fd->rendez);
        
        if (fd->msg_sz < 0)
            fd->exit_status = 1;
    }
}






/*
 *  Task for an open file descriptor / filp
 */

void task_filedesc (void *arg)
{
    struct Filedesc *fd = arg;
    
    while (fd->exit_status == 0)
    {
        while (fd->msg_sz = GetMsg (fd->handle, &fd->msg, PROT_READWRITE) > 0)
        {
            switch (tf->msg->cmd)
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
				
				case EXEC_CMD_SYMLINK:
					fd->symlink();
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
				
				case EXEC_CMD_RELABEL:
					fd->relabel();
					break;
					
				case EXEC_CMD_FORMAT:
					fd->format();
					break;
					
				case EXEC_CMD_GETVOLUMEGUID:
					fd->getvolumeguid();
					break;
					
				case EXEC_CMD_SETVOLUMEGUID:
					fd->setvolumeguid();
					break;
					
				case EXEC_CMD_READDEVICEENTRY:
					fd->ReadDeviceEntry();
					break;
					
				case EXEC_CMD_READPARTITIONTABLE:
					fd->ReadPartitionTable();
					break;
					
				case EXEC_CMD_WRITEPARTITIONTABLE:
					fd->WritePartitionTable();
					break;
					
				case EXEC_CMD_INSTALLMBRBOOTLOADER:
					fd->InstallMBRBootLoader();
					break;
				
				case EXEC_CMD_SETREMOVABLEATTRS:
					fd->SetRemovableAttrs();
					break;
				
				case EXEC_CMD_SETPROGRAMDIR:
					fd->SetProgramDir();
					break;
				
				case EXEC_CMD_DUP:
					fd->Dup();
					break;
					
				case EXEC_CMD_STARTNOTIFY:
					fd->StartNotify();
					break;
				
				case EXEC_CMD_ENDNOTIFY:
					fd->EndNotify();
					break;
				
				case EXEC_CMD_WAITNOTIFY:
					fd->WaitNotify();
					break;
									
				default:
					fd->exit_status = 1;
					break;
			}
			
			iov[0] = fd->msg;
			PutMsg (fd->handle, iov, 1);
		}
		
        while (fd->msg_sz == 0 && fd->exit_status != 0)
            tasksleep (&fd->rendez);

        if (fd->msg_sz < 0)
            fd->exit_status = 1;
    }
    
    
    // Free any messages or memory
    // close this channel
    // free filedesc,
    // decrement vnode reference count.
}









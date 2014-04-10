#include <sys/types.h>
#include <sys/syscalls.h>
#include <interfaces/filesystem.h>
#include "filesystem.h"





/*
 *  Task for an open file descriptor / filp
 */

void SystemDesc::SystemDescTask (void *arg)
{
    class SystemDesc *fd = (class SystemDesc *)arg;
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
        else if (msg_sz < 0 || msg_sz < (ssize_t) sizeof *msg)
        {
            exit_status = 1;
        }
        else
        {
            switch (msg->cmd)
			{	
                case  EXEC_CMD_SETUID:
                    break;
                
                case EXEC_CMD_GETUID:
                    break;
                    
                case EXEC_CMD_SETGID:
                    break;
                    
                case EXEC_CMD_GETGID:
                    break;
                    
                case EXEC_CMD_CREATEPORT:
                    break;
                    
                case EXEC_CMD_LISTEN:
                    break;
                    
                case EXEC_CMD_CONNECT:
                    break;
         
                default:
                    break;
            }
        }
    }
}











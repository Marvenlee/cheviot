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
#include <kernel/vm.h>
#include <string.h>


void LogFDs(void)
{
  struct Filp *filp;
  struct VNode *vnode;
  
  char buf[30];
  
  for (int t=0; t<6; t++)
  {  
    filp = get_filp(t);
    
    if (filp)
    {
        vnode = filp->vnode;
     
        if (S_ISCHR(vnode->mode)) {  
          Info ("FD[%d]: ISCHR",t);      
        } else if (S_ISREG(vnode->mode)) {
          Info ("FD[%d]: ISREG, sz:%d",t, vnode->size);
        } else if (S_ISFIFO(vnode->mode)) {
          Info ("FD[%d]: ISFIFO",t);    
        } else if (S_ISBLK(vnode->mode)) {
          Info ("FD[%d]: ISBLK",t);  
        } else if (S_ISDIR(vnode->mode)) {
          Info ("FD[%d]: ISDIR",t);  
        } else {
          Info ("FD[%d]: unknown",t);  
        }
    }
    else
    {
      Info ("FD[%d]: -----", t);
    }
  }
}



/*
 * Fcntl();
 */

SYSCALL int sys_fcntl (int fd, int cmd, int arg)
{
  struct Process *current;
  struct Filp *filp;
	int new_fd;

  Info("SysFcntl(%d, %d)", fd, cmd);
  LogFDs();
  
  current = get_current_process();

  if ((filp = get_filp(fd)) == NULL) {
    Info ("Fcntl fd %d does not exist", fd);
    return -EINVAL;
  }
  
  if (current->fd_table[fd] == NULL) {
    Info ("Fcntl FD FREE");
    return -EBADF;
  }

	switch(cmd)
	{
      case F_DUPFD:	/* Duplicate fildes */
  	    if (arg < 0 || arg >= NPROC_FD) {
          Info ("Fcntl F_DUPFD -EBADF");
          return -EBADF;
        }

	      for (new_fd = arg; new_fd < NPROC_FD; new_fd++)
	      {
	        if (current->fd_table[new_fd] == NULL) {
              current->fd_table[new_fd] = current->fd_table[fd];            
              filp->reference_cnt++;          
              
              Info ("fcntl dup, new_fd: %d", new_fd);
              return new_fd;  
          }
        }
        
        Info ("Fcntl F_DUPFD -ENOMEM");        
        return -ENOMEM;
    
		
      case F_GETFD:	/* Get fildes flags */	  
	    if (current->close_on_exec[fd/32] & (1<<(fd%32))) {
        Info ("Fcntl F_GETFD FD_CLOEXEC");
	      return 1;
		} else {			    
        Info ("Fcntl F_GETFD 0");			  
		    return 0;
		}
		  
		case F_SETFD:	/* Set fildes flags */
		  if (arg) {
        current->close_on_exec[fd/32] |= (1<<(fd%32));
		  } else {
        current->close_on_exec[fd/32] &= ~(1<<(fd%32));
      }

			return arg;
		
		case F_GETFL:	/* Get file flags */
      Info ("Fcntl F_GETFL");
      break;
      
		case F_SETFL:	/* Set file flags */
      Info ("Fcntl F_SETFL");
      break;
      
		default:
		  Info ("Fcntl ENOSYS");
			return -ENOSYS;
	}

  Info ("Fcntl -EINVAL");
	return -EINVAL;
}


SYSCALL int sys_dup(int fd) {
  int new_fd;
  struct Filp *filp;
  struct Process *current;

  Info("Dup: %d", fd);
  LogFDs();

  current = get_current_process();

  filp = get_filp(fd);

  if (filp == NULL) {
    Info ("Dup failed, EINVAL");
    return -EINVAL;
  }


  for (new_fd = 0; new_fd < NPROC_FD; new_fd++) {
    if (current->fd_table[new_fd] == NULL) {
      current->fd_table[new_fd] = current->fd_table[fd];  
      filp->reference_cnt++;

      Info ("%d = SysDup (%d)", new_fd, fd);
      LogFDs();

      return new_fd;
    }
  }

  Info ("Dup failed, -ENOMEM");
  return -ENOMEM;
}

/**
 *
 */
SYSCALL int sys_dup2(int fd, int new_fd) {
  struct Filp *filp;
  struct Process *current;

  Info ("SysDup2 (%d, %d)", fd, new_fd);
  LogFDs();
  
  current = get_current_process();

  if (new_fd < 0 || new_fd >= NPROC_FD) {
    Info ("Dup2 : new_h out of range");
    return -EINVAL;
  }

  filp = get_filp(fd);

  if (filp == NULL) {
    Info ("dup2: filp = null");
    return -EINVAL;
  }

  if (current->fd_table[new_fd] != NULL) {
//    Info ("**** Closing existing file %d", new_fd);
    sys_close(new_fd);
  }

  current->fd_table[new_fd] = current->fd_table[fd];
  filp->reference_cnt++;
  Info ("Dup2 done, fd:%d, new_fd:%d", fd, new_fd);
  LogFDs();

  return new_fd;
}


SYSCALL int sys_close(int fd)
{
  struct Filp *filp;
  struct Pipe *pipe;
  struct VNode *vnode;

  Info("SysClose(%d)", fd);
  LogFDs();
  
  filp = get_filp(fd);

  if (filp == NULL) {
    Info ("CLose failed, filp = NULL");
    return -EINVAL;
  }

  vnode = filp->vnode;
  
  if (vnode != NULL) {
    
    vnode_lock(vnode);
    
    if (S_ISFIFO(vnode->mode)) {
      Info ("*** Do close pipe, wakeup rendez");
      pipe = vnode->pipe;    
      TaskWakeupAll(&pipe->rendez);
    }

    Info ("SysClose vnode_put = %08x", vnode);
    vnode_put(vnode);
  }
  else
  {
    Info("Close vnode is NULL");
  }
  
  free_fd(fd);
  
  Info("SysClose FDs...");
  LogFDs();

  return 0;
}

/**
 *
 */
static struct Filp *alloc_filp(void) {
  struct Filp *filp;

  filp = LIST_HEAD(&filp_free_list);

  if (filp == NULL) {
    return NULL;
  }

  LIST_REM_HEAD(&filp_free_list, filp_entry);
  filp->reference_cnt = 0;
  filp->vnode = NULL;
  return filp;
}

/**
 *
 */
static void free_filp(struct Filp *filp) {

  if (filp == NULL) {
    return;
  }

  filp->reference_cnt--;
  
  if (filp->reference_cnt == 0) {
    if (filp->vnode != NULL) {
      filp->vnode->reference_cnt--;    
    }

    filp->vnode = NULL;
    LIST_ADD_HEAD(&filp_free_list, filp, filp_entry);
  }
}

/*
 * Returns a pointer to the object referenced by the handle 'fd'.  Ensures
 * it belongs to the specified process and is of the requested type.
 */
struct Filp *get_filp(int fd) {
  struct Process *current;
  uint16_t filp_idx;

  current = get_current_process();

  if (fd < 0 || fd >= NPROC_FD) {
    return NULL;
  }

  return current->fd_table[fd];
}

/*
 * Allocates a handle structure.  Checks to see that free_handle_cnt is
 * non-zero should already have been performed prior to calling alloc_fd().
 */
int alloc_fd(void) {
  int fd;
  struct Process *current;
  struct Filp *filp;
  
  current = get_current_process();
  
  for (fd = 0; fd < NPROC_FD; fd++) {
    if (current->fd_table[fd] == NULL) {
      filp = alloc_filp();
      
      if (filp == NULL) {
        return -ENOMEM;
      }
      
      current->fd_table[fd] = filp;      
      filp->reference_cnt++;
      return fd;
    }
  }

  return -EMFILE;
}

/*
 * Returns a handle to the free handle list and increments the free_handle_cnt.
 */
int free_fd(int fd) {
  struct Process *current;
  struct Filp *filp;
  
  Info ("FreeHandle(%d)", fd);
  
  current = get_current_process();
  
  if (fd < 0 || fd >= NPROC_FD) {
    Info("FreeHandle %d EINVAL", fd);
    return -EINVAL;
  }

  filp = get_filp(fd);
  free_filp(filp);
  current->fd_table[fd] = NULL;
  return 0;
}

/*
 *
 */
void init_process_fds(struct Process *proc)
{
  for (int fd = 0; fd < NPROC_FD; fd++) {
    proc->fd_table[fd] = NULL;
  }
  
  for (int t=0; t < NPROC_FD/32; t++)
  {
    proc->close_on_exec[t] = 0x00000000;
  }
}

/*
 *
 */
void close_process_fds(struct Process *proc)
{
  Info ("FreeProcessHandles()");
  
  for (int fd = 0; fd < NPROC_FD; fd++) {
    if (proc->fd_table[fd] != NULL) {
      sys_close(fd);
    }
  }
}

/*
 *
 */
int fork_process_fds(struct Process *newp, struct Process *oldp) {
  int fd;
  uint16_t filp_idx;
  
  Info ("ForkProcessHandles()");
  
  for (fd = 0; fd < NPROC_FD; fd++) {
    newp->fd_table[fd] = oldp->fd_table[fd];

    if (newp->fd_table[fd] != NULL) {
        newp->fd_table[fd]->reference_cnt++;
    }
  }
  
  newp->current_dir = oldp->current_dir;
  
  if (newp->current_dir != NULL) {
    vnode_inc_ref(newp->current_dir);
  }

  return 0;
}

int close_on_exec_process_fds(void) {
  struct Process *current;

  Info("CloseOnExecProcessHandles()");
  current = get_current_process();

  for (int fd = 0; fd < NPROC_FD; fd++) {
    if (current->close_on_exec[fd/32] & (1<<(fd%32))) {
      sys_close(fd);
    }
  }

  vnode_lock(current->current_dir);
  vnode_put(current->current_dir);

  return 0;
}


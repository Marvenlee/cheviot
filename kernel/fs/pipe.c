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

#define KDEBUG

#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>


struct SuperBlock pipe_sb;


/*
 *
 */
void InitPipes(void)
{
  struct Pipe *pipe;
  
  LIST_INIT(&free_pipe_list);
  
  for (int t=0; t<max_pipe; t++)
  {
    LIST_ADD_TAIL(&free_pipe_list, &pipe_table[t], link);
  }
}


/*
 *
 */
struct Pipe *AllocPipe(void)
{
  struct Pipe *pipe;
    
  pipe = LIST_HEAD(&free_pipe_list);
  
  if (pipe == NULL) {
    Info("AllocPipe, failed, list empty");
    return NULL;
  }
  
  LIST_REM_HEAD(&free_pipe_list, link);

  pipe->w_pos = 0;
  pipe->r_pos = 0;
  pipe->data_sz = 0;
  pipe->free_sz = PIPE_BUF_SZ;
  
  pipe->reader_cnt = 0;
  pipe->writer_cnt = 0;
  pipe->data = (uint8_t *)pipe_mem_base + (pipe - pipe_table) * PIPE_BUF_SZ;
  InitRendez(&pipe->rendez);
  
  return pipe;
}


/*
 *
 */
void FreePipe(struct Pipe *pipe)
{
  if (pipe != NULL) {
    return;
  }

  LIST_ADD_HEAD(&free_pipe_list, pipe, link);
}


/*
 *
 */
SYSCALL int sys_pipe(int _fd[2])
{
  int fd[2] = {-1, -1};
  struct Filp *filp0 = NULL;
  struct Filp *filp1 = NULL;
  struct VNode *vnode = NULL;
  struct Buf *buf = NULL;
  struct Process *current;
  int error;
  struct Pipe *pipe;  
  
 
  Info("sys_pipe: _fd=%08x", _fd);
  
  current = get_current_process();

  vnode = vnode_new(&pipe_sb, 0);
  
  if (vnode == NULL) {
    error = -ENOMEM;
    goto exit;
  }
  
  pipe = AllocPipe();

  if (pipe == NULL) {
    error = -ENOMEM;
    goto exit;
  }


  fd[0] = alloc_fd();

  if (fd[0] < 0) {
    error = -ENOMEM;
    goto exit;
  }
  
  fd[1] = alloc_fd();
  if (fd[1] < 0) {
    error = -ENOMEM;
    goto exit;
  }
  
  Info ("sys_pipe alloced fds: %d, %d", fd[0], fd[1]);
  
  vnode->mode = _IFIFO | 0777;
  vnode->superblock = &pipe_sb;
  vnode->pipe = pipe;
  
  filp0 = get_filp(fd[0]);
  filp0->vnode = vnode;
  filp0->offset = 0;

  filp1 = get_filp(fd[1]);
  filp1->vnode = vnode;
  filp1->offset = 0;

  vnode->reference_cnt = 2;

  vnode_unlock(vnode);

  if (CopyOut(_fd, fd, sizeof fd) != 0) {
    Info("Pipe, failed, EFAULT");
    error = -EFAULT;
    goto exit;
  }

  Info ("sys_pipe success, fd[0] = %d, fd[1] = %d", fd[0], fd[1]);
  
  return 0;
  
exit:

  // TODO: Need to check these aren't null or -1 when freeing
  Info ("Pipe failed error = %d", error);
  
/*  FreePipe(pipe);
  vnode_Free(vnode);
  FreeFilp(filp1);
  FreeFilp(filp0);
  free_fd(fd[1], current);
  free_fd(fd[0], current);
*/
  return error;
}


/*
 * Do we use same code for pipes as well as socketpair devices?
 */

ssize_t ReadFromPipe(struct VNode *vnode, void *_dst, size_t sz)
{
  uint8_t *dst = (uint8_t *)_dst;
  uint8_t *dst2;
  int sz1;
  int sz2;
  size_t remaining;
  ssize_t nbytes_to_copy;
  ssize_t nbytes_read = 0;   
  int status = 0;
  struct Pipe *pipe;
  
  Info("ReadFromPipe, sz = %d", sz);

  pipe = vnode->pipe;
  
  while (nbytes_read == 0 && status == 0) {         
    remaining = sz - nbytes_read;

    Info ("..Pipe read remaining = %d, data_sz = %d, ref = %d", remaining, pipe->data_sz, vnode->reference_cnt);
      
    while (pipe->data_sz == 0 && vnode->reference_cnt > 1) {
      Info ("..Pipe read sleeping");
      TaskSleep (&pipe->rendez);
    }

    if ( vnode->reference_cnt <= 1 && pipe->data_sz == 0) {
      Info ("..Pipe read, ref_cnt = %d, data_sz=%d", vnode->reference_cnt, pipe->data_sz);
      break;
    }  

    nbytes_to_copy = (remaining < pipe->data_sz) ? remaining : pipe->data_sz;

    if ((pipe->r_pos + nbytes_to_copy) > PIPE_BUF_SZ) {
      sz1 = PIPE_BUF_SZ - pipe->r_pos;

      if (CopyOut (dst, pipe->data + pipe->r_pos, sz1) != 0) {
        status = -EIO;
        break;
      }

      dst2 += sz1;
      sz2 = nbytes_to_copy - sz1;
    } else {
      dst2 = dst;
      sz2 = nbytes_to_copy;
    }
    
    if (sz2) {
      if (CopyOut (dst, pipe->data, sz2) != 0) {
        status = -EIO;
        break;
      }
    }
  
    pipe->r_pos = (pipe->r_pos + nbytes_to_copy) % PIPE_BUF_SZ;
    pipe->data_sz -= nbytes_to_copy;
    pipe->free_sz += nbytes_to_copy;  
    dst += nbytes_to_copy;
    nbytes_read += nbytes_to_copy;
   
    TaskWakeupAll (&pipe->rendez);
  }

  Info ("..Pipe read, read:%d, st:%d", nbytes_read, status);    
  return (status == 0) ? nbytes_read : status;
}


/*
 *
 */

ssize_t WriteToPipe(struct VNode *vnode, void *_src, size_t sz)
{
  uint8_t *src = (uint8_t *)_src;
  uint8_t *src2;
  size_t sz1;
  size_t sz2;
  size_t remaining;
  ssize_t nbytes_to_copy;
  ssize_t nbytes_written = 0;
  int status = 0;
  struct Pipe *pipe;
  
  Info("WriteToPipe, sz = %d", sz);

  pipe = vnode->pipe;

  while (nbytes_written == 0 && status == 0) {
    remaining = sz - nbytes_written;

    Info ("..Pipe write remaining=%d, free_sz=%d, ref=%d", remaining, pipe->free_sz, vnode->reference_cnt);

    while (pipe->free_sz < PIPE_BUF && vnode->reference_cnt > 1) {
      Info ("..Pipe write sleeping");
      TaskSleep (&pipe->rendez);
    }

    if ( vnode->reference_cnt <= 1) {
      Info ("Pipe write, ref_cnt = %d, ending xfer", vnode->reference_cnt);
      // FIXME: pipe reference count ends up being -5
      break;
    }  

    nbytes_to_copy = (remaining < pipe->free_sz) ? remaining : pipe->free_sz;
       
    if ((pipe->w_pos + nbytes_to_copy) > PIPE_BUF_SZ) {
      sz1 = PIPE_BUF_SZ - pipe->w_pos;   

      if (CopyIn (pipe->data + pipe->w_pos, src, sz1) != 0) {
        status = -EIO;
        break;
      }
      
      src2 = src + sz1;
      sz2 = nbytes_to_copy - sz1;
    } else {
      src2 = src;
      sz2 = nbytes_to_copy;
    }
  
    if (sz2) {
      if (CopyIn (pipe->data, src2, sz2) != 0) {
        status = -EIO;
        break;
      } 
    }
      
    pipe->w_pos = (pipe->w_pos + nbytes_to_copy) % PIPE_BUF_SZ; 
    pipe->data_sz += nbytes_to_copy;
    pipe->free_sz -= nbytes_to_copy;
    src += nbytes_to_copy;
    nbytes_written += nbytes_to_copy;

    TaskWakeupAll (&pipe->rendez);    
  }

  Info ("..pipe write, wrote:%d, st:%d", nbytes_written, status);
  return (status == 0) ? nbytes_written : status; 
}


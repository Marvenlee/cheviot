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


/* @brief   Lookup a file pointer from a file descriptor
 *
 * @param   proc, process that the file descriptor belongs to
 * @param   fd, file descriptor to look up
 * @return  filp (file pointer) structure
 */
struct Filp *get_filp(struct Process *proc, int fd)
{
  if (fd < 0 || fd >= OPEN_MAX) {
    return NULL;
  }
  
  return proc->fproc->fd_table[fd];
  
}


/* @brief   Allocate a file descriptor and filp
 *
 * Checks to see that free_handle_cnt is non-zero should already have
 * been performed prior to calling alloc_fd().
 */
int alloc_fd_filp(struct Process *proc)
{
  int fd;
  struct Filp *filp;
  
  fd = alloc_fd(proc, 0, OPEN_MAX);
  
  if (fd < 0) {
    return -EMFILE;
  }
  
  filp = alloc_filp();
  
  if (filp == NULL) {
    free_fd(proc, fd);
    return -EMFILE;
  }
  
  filp->reference_cnt=1;
  
  proc->fproc->fd_table[fd] = filp;
  filp->type = FILP_TYPE_UNDEF;  
  return fd;
}


/* @brief   Free a file descriptor and filp
 */
int free_fd_filp(struct Process *proc, int fd)
{
  struct Filp *filp;
  
  filp = get_filp(proc, fd);

  if (filp == NULL) {
    return -EINVAL;
  }

  free_filp(filp);
  free_fd(proc, fd);
  return 0;
}


/* @brief   Allocate a filp (file pointer)
 */
struct Filp *alloc_filp(void)
{
  struct Filp *filp;

  filp = LIST_HEAD(&filp_free_list);

  if (filp == NULL) {
    return NULL;
  }

  LIST_REM_HEAD(&filp_free_list, filp_entry);
  filp->reference_cnt = 1;
  filp->type = FILP_TYPE_UNDEF;
  memset(&filp->u, 0, sizeof filp->u);
  return filp;
}


/* @brief   Free a filp (file pointer)
 */
void free_filp(struct Filp *filp)
{
  if (filp == NULL) {
    return;
  }

  filp->reference_cnt--;
 
  if (filp->reference_cnt == 0) {
    filp->type = FILP_TYPE_UNDEF;
    memset(&filp->u, 0, sizeof filp->u);
    LIST_ADD_HEAD(&filp_free_list, filp, filp_entry);
  }
}


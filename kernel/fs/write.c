
//#define KDEBUG

#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/vm.h>
#include <sys/fsreq.h>
#include <sys/mount.h>


/* @brief   Write the contents of a buffer to a file
 *
 * @param   fd, file descriptor of file to write to
 * @param   src, user-mode buffer containing data to write to file
 * @param   sz, size in bytes of buffer pointed to by src
 * @return  number of bytes written or negative errno on failure
 */
ssize_t sys_write(int fd, void *src, size_t sz)
{
  struct Filp *filp;
  struct VNode *vnode;
  ssize_t xfered;
  struct Process *current;
  
  current = get_current_process();
  filp = get_filp(current, fd);
  vnode = get_fd_vnode(current, fd);

  if (vnode == NULL) {
    return -EINVAL;
  }

  if (is_allowed(vnode, W_OK) != 0) {
    return -EACCES;
  }
  
  #if 0   // FIXME: Check if writer permission  
  if (filp->flags & O_WRITE) == 0) {
    return -EACCES;
  } 
  #endif  

  
  vnode_lock(vnode);
  
  // TODO: Add write to cache path
  if (S_ISCHR(vnode->mode)) {
    xfered = write_to_char(vnode, src, sz);  
  } else if (S_ISBLK(vnode->mode)) {
    xfered = write_to_block(vnode, src, sz, &filp->offset);
  } else if (S_ISFIFO(vnode->mode)) {
    xfered = write_to_pipe(vnode, src, sz);
  } else {
    xfered = -EINVAL;
  }  

  // TODO: Update accesss timestamps
  vnode_unlock(vnode);
  
  return xfered;
}



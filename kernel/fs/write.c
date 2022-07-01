
//#define KDEBUG

#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/vm.h>
#include <sys/fsreq.h>
#include <sys/mount.h>




/*
 *
 */
SYSCALL ssize_t sys_write(int fd, void *src, size_t sz) {
  struct Filp *filp;
  struct VNode *vnode;
  ssize_t xfered;
  
  Info("sys_write fd:%d, src:%08x, sz:%d", fd, src, sz);
  
  filp = get_filp(fd);

  if (filp == NULL) {
    Info ("write - filp is null");
    return -EINVAL;
  }

  vnode = filp->vnode;

  if (is_allowed(vnode, W_OK) != 0) {
    Info ("Write IsAllowed failed");
    return -EACCES;
  }
  
  vnode_lock(vnode);
  
  if (S_ISCHR(vnode->mode)) {
    xfered = write_to_char(vnode, src, sz);  
  } else if (S_ISBLK(vnode->mode)) {
    xfered = write_to_cache(vnode, src, sz, &filp->offset);
  } else if (S_ISFIFO(vnode->mode)) {
    xfered = WriteToPipe(vnode, src, sz);
  } else {
    Info ("Write to unknown file type");
    xfered = -EINVAL;
  }  

  // Update accesss timestamps

  vnode_unlock(vnode);
  
  return xfered;
}














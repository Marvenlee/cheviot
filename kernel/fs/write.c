
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
SYSCALL ssize_t SysWrite(int fd, void *src, size_t sz) {
  struct Filp *filp;
  struct VNode *vnode;
  ssize_t xfered;
  
  filp = GetFilp(fd);

  if (filp == NULL) {
    return -EINVAL;
  }

  vnode = filp->vnode;

  if (IsAllowed(vnode, W_OK) != 0) {
    Info ("Write IsAllowed failed");
    return -EACCES;
  }
  
  
  if (S_ISCHR(vnode->mode)) {
    xfered = WriteToChar(vnode, src, sz);  
  } else if (S_ISBLK(vnode->mode)) {
    xfered = WriteToCache(vnode, src, sz, &filp->offset);
  } else if (S_ISFIFO(vnode->mode)) {
    xfered = WriteToPipe(vnode, src, sz, &filp->offset);
  } else {
    Info ("Write to unknown file type");
    xfered = -EINVAL;
  }  
  
  return xfered;
}














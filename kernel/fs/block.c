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
#include <kernel/vm.h>
#include <sys/fsreq.h>
#include <sys/mount.h>


/* @brief   Read from a block device
 * 
 * TODO: Avoid in-kernel buffer, readmsg/writemsg needs to be able to write
 * to client process directly.
 */
ssize_t read_from_block (struct VNode *vnode, void *dst, size_t sz, off64_t *offset)
{
  uint8_t data[512];
  ssize_t xfered = 0;
  size_t xfer = 0;
  size_t remaining = sz;

  while (remaining > 0) {
    xfer = (remaining < sizeof data) ? remaining : sizeof data;
    xfered = vfs_read(vnode, data, xfer, offset);      
    
    if (xfered == 0) {
      return sz - remaining;
    }
    
    if (xfered < 0) {
      Error("read_from_block err:%d", xfered);
      return xfered;
    }

    CopyOut(dst, data, xfered);
    dst += xfered;  
    remaining -= xfered;
  }

  return sz - remaining;
}


/* @brief   Write to a block device
 */
ssize_t write_to_block (struct VNode *vnode, void *src, size_t sz, off64_t *offset)
{
  uint8_t data[512];
  ssize_t xfered = 0;
  size_t xfer = 0;
  size_t remaining = sz;
  
  while (remaining > 0) {      
    xfer = (remaining < sizeof data) ? remaining : sizeof data;
    CopyIn(data, src, xfer);
    xfered = vfs_write(vnode, data, xfer, offset);      

    if (xfered == 0) {
      return sz - remaining;
    }
    
    if (xfered < 0) {
      return xfered;
    }

    src += xfered;
    remaining -= xfered;
  }
  
  return sz - remaining;
}


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

// TODO
// ReadFromBlock
// WriteToBlock
//
// Cache/double buffer 64k-sized blocks.
// Need to mark which sub-blocks have changed for writing.
//
// FIXME: Could we use blocks and block size for block mounted devices ?
//  blksize_t     st_blksize;
//  blkcnt_t	st_blocks;


ssize_t read_from_block (struct VNode *vnode, void *dst, size_t sz, off64_t *offset)
{
    uint8_t buf[512];
    ssize_t xfered = 0;
    size_t xfer = 0;
    struct Process *current;

    current = get_current_process();

    Info ("ReadFromBlock(sz:%d, offs:%08x)", sz, *offset);
      
    xfer = (sz < sizeof buf) ? sz : sizeof buf;

    xfered = vfs_read(vnode, buf, xfer, offset);
        
    CopyOut(dst, buf, xfered);
       
    Info ("ReadFromBlock xfered: %d", xfered);

    return xfered;
}

/*
 * TODO: write to block device
 */
ssize_t write_to_block (struct VNode *vnode, void *dst, size_t sz, off64_t *offset)
{
    return -ENOSYS;
}


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

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <poll.h>
#include <kernel/proc.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/dbg.h>
#include <poll.h>


/* @brief   Send a signal to processes with a particular open file
 *
 * @param   fd, file handle of the mount point created by sys_mount()
 * @param   ino, inode number of file whose processes with it open shall receive signal
 * @param   signal, signal to raise
 *
 * This system call is intended for the TTY driver to be able to send
 * signals to client processes in response to keyboard inputs such
 * as SIGTERM, SIGKILL, etc.
 *
 * TODO: sys_signalnotify implementation
 *
 * This may change to only support character devices, the "ino" parameter will
 * then be deprecated.
 */
int sys_signalnotify(int fd, int ino, int signal)
{
  return -ENOSYS;
}




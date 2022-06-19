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

/*
 * TODO: Used by device drivers to send a signal to a client, 
 */
SYSCALL int sys_signalnotify(int fd, int ino, int signal)
{
  return -ENOSYS;
}


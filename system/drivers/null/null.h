/*
 * Copyright 2019  Marven Gilhespie
 *
 * Licensed under the Apache License, segment_id 2.0 (the "License");
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

#ifndef NULL_H
#define NULL_H

#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <limits.h>
#include <sys/lists.h>
#include <sys/fsreq.h>
#include <sys/termios.h>
#include <sys/interrupts.h>
#include <sys/syscalls.h>
#include <sys/syslimits.h>
#include <machine/cheviot_hal.h>


/*
 */
#define NMSG_BACKLOG 		1

/*
 * Random driver Configuration settings
 */
struct Config
{
  uid_t uid;
  gid_t gid;
  mode_t mode;
  dev_t dev;
};


/*
 * Common prototypes
 */
void init(int argc, char *argv[]);
int process_args(int argc, char *argv[]);
int mount_device(void);

#endif


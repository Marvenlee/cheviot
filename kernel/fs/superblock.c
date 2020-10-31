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

#include <kernel/arm/boot.h>
#include <kernel/dbg.h>
#include <kernel/filesystem.h>
#include <kernel/globals.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>
#include <string.h>

struct SuperBlock *AllocSuperBlock(void) {
  struct SuperBlock *sb;

  sb = LIST_HEAD(&free_superblock_list);

  if (sb != NULL) {
    LIST_REM_HEAD(&free_superblock_list, link);
    memset(sb, 0, sizeof *sb);
    InitRendez (&sb->rendez);
    sb->dev = sb - superblock_table;
  }

  return sb;
}

void FreeSuperBlock(struct SuperBlock *sb) {
  if (sb == NULL) {
    return;
  }

  LIST_ADD_TAIL(&free_superblock_list, sb, link);
}

void LockSuperBlock(struct SuperBlock *sb) {
  while (sb->busy == TRUE) {
    TaskSleep(&sb->rendez);
  }

  sb->busy = TRUE;
}

void UnlockSuperBlock(struct SuperBlock *sb) {
  sb->busy = FALSE;
  TaskWakeupAll(&sb->rendez);
}

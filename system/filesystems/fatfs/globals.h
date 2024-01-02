/*
 * Copyright 2014  Marven Gilhespie
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

#ifndef GLOBALS_H
#define GLOBALS_H

#include "fat.h"
#include <sys/syscalls.h>
#include <sys/types.h>


extern struct FatSB fsb;

extern uint8 bootsector[512];

extern int fat_buf_sector;
extern uint8 fat_buf[512];

extern int file_buf_sector;
extern uint8 file_buf[512];

extern uint32 partition_start;

extern uint8_t dirents_buf[DIRENTS_BUF_SZ];

extern uint8 read_buf[0x10000];
extern uint8 write_buf[512];

extern int portid;
extern int kq;

extern int block_fd;

extern struct Cache *block_cache;

extern struct Config config;

#endif

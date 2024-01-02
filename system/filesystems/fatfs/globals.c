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

#include "globals.h"
#include "fat.h"
#include <sys/syscalls.h>
#include <sys/types.h>


struct FatSB fsb;

uint8 bootsector[512];

int file_buf_sector;
uint32 canary1 = 0xdeadbeef;
uint8 file_buf[512];
uint32 canary2 = 0xdeadbeef;

uint8_t dirents_buf[DIRENTS_BUF_SZ];

uint8 read_buf[0x10000];
uint8 write_buf[512];

int portid;
int kq;
int block_fd = -1;

struct Cache *block_cache;

struct Config config;


struct Fat12BPBSpec fat12_bpb[] =
{
   {512, 1, 1, 2,  64,  320, 0xfe, 1,  8, 1},   /* 160K */ 
   {512, 1, 1, 2,  64,  360, 0xfc, 2,  9, 1},   /* 180K */ 
   {512, 2, 1, 2, 112,  640, 0xff, 1,  8, 2},   /* 320K */ 
   {512, 2, 1, 2, 112,  720, 0xfd, 2,  9, 2},   /* 360K */ 
   {512, 2, 1, 2, 112, 1440, 0xf9, 3,  9, 2},   /* 720K */ 
   {512, 1, 1, 2, 224, 2400, 0xf9, 7, 15, 2},   /* 1.2M */ 
   {512, 1, 1, 2, 224, 2880, 0xf0, 9, 18, 2},   /* 1.44M */ 
   {512, 2, 1, 2, 240, 5760, 0xf0, 9, 36, 2}    /* 2.88M */
};


struct FatDskSzToSecPerClus dsksz_to_spc_fat16[] =
{
	{      8400,   0},   /* <= 4.1MB,   Invalid    */
	{     32680,   2},   /* <=  16MB,   1k cluster */
	{    262144,   4},   /* <= 128MB,   2k cluster */
	{    524288,   8},   /* <= 256MB,   4k cluster */
	{   1048576,  16},   /* <= 512MB,   8k cluster */
	{0xffffffff,   0}    /* >  512MB,   Invalid    */
};


struct FatDskSzToSecPerClus dsksz_to_spc_fat32[] =
{
	{     66600,   0},   /* <= 32.5MB,   Invalid      */
	{    532480,   1},   /* <=  260MB,   0.5k cluster */
	{  16777216,   8},   /* <=    8GB,   4k cluster   */
	{  33554432,  16},   /* <=   16GB,   8k cluster   */
	{  67108864,  32},   /* <=   32MB,   16k cluster  */
	{0xffffffff,  64}    /* >    32MB,   32k cluster  */
};


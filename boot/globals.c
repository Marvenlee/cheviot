#include "types.h"
#include "globals.h"
#include "block.h"
#include "elf.h"
#include "fat.h"
#include "memory.h"
#include "bootinfo.h"

struct BootInfo bootinfo;

struct Module *module_table;
int module_cnt;
void *module_heap;

vm_addr user_base;

struct Segment segment_table[NSEGMENT];
int segment_cnt;

Elf32_EHdr ehdr;
Elf32_PHdr phdr_table[MAX_PHDR];
int phdr_cnt;

Elf32_PHdr kernel_phdr_table[MAX_PHDR];
int kernel_phdr_cnt;

Elf32_PHdr procman_phdr_table[MAX_PHDR];
int procman_phdr_cnt;




struct block_device *bdev;	

struct FatSB fsb;
struct FatFilp filp;
struct FatNode node;

//int32 current_fat_sector;
//uint32 lseek_fat_entry;

uint8 bootsector[512];
//uint8 bpb_sector[512];

int fat_buf_sector;
uint8 fat_buf[512];

int file_buf_sector;
uint8 file_buf[512];


/*
 *
 */
 
 uint32 arm_mem_base;
 uint32 arm_mem_size;
 uint32 vc_mem_base;
 uint32 vc_mem_size;
	
	





/* Debugger stuff */

 volatile uint32 *gpio;
 uint32 screen_width;
 uint32 screen_height;
 uint32 screen_buf;
 uint32 screen_pitch;

 bool __debug_enabled;




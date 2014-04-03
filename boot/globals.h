#ifndef GLOBALS_H
#define GLOBALS_H


#include "types.h"
#include "block.h"
#include "elf.h"
#include "memory.h"
#include "bootinfo.h"

extern struct BootInfo bootinfo;

extern vm_addr user_base;

extern struct Segment segment_table[NSEGMENT];
extern int segment_cnt;

extern struct Module *module_table;
extern int module_cnt;
extern void *module_heap;

extern Elf32_EHdr ehdr;
extern Elf32_PHdr phdr_table[MAX_PHDR];
extern int phdr_cnt;

extern Elf32_PHdr kernel_phdr_table[MAX_PHDR];
extern int kernel_phdr_cnt;

extern Elf32_PHdr procman_phdr_table[MAX_PHDR];
extern int procman_phdr_cnt;


extern struct block_device *bdev;

extern struct FatSB fsb;
extern struct FatFilp filp;
extern struct FatNode node;

//extern int32 current_fat_sector;
//extern uint32 lseek_fat_entry;


extern uint8 bootsector[512];
//extern uint8 bpb_sector[512];

int fat_buf_sector;
extern uint8 fat_buf[512];

int file_buf_sector;
extern uint8 file_buf[512];

extern uint32 partition_start;


/*
 *
 */
 
 
 
extern uint32 mailbuffer[64];
extern uint32 arm_mem_base;
extern uint32 arm_mem_size;
extern uint32 vc_mem_base;
extern uint32 vc_mem_size;
	


/* Debugger stuff */

extern volatile uint32 *gpio;
extern uint32 screen_width;
extern uint32 screen_height;
extern uint32 screen_buf;
extern uint32 screen_pitch;

extern bool __debug_enabled;



#endif

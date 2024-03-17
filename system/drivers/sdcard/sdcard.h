/* Copyright (C) 2013 by John Cronin <jncronin@tysos.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef BLOCK_H
#define BLOCK_H

#include <stddef.h>
#include <stdint.h>
#include <sys/fsreq.h>
#include <sys/syslimits.h>
#include <sys/syscalls.h>
#include "peripheral_base.h"

/*
 */
#define NMSG_BACKLOG 				1
#define BUF_SZ    			 4096      // Buffer size used to read and write

typedef uint64_t  block64_t;

// @brief   structure representing a mount point, e.g. sda, sda1, sda2, sda3 or sda4
struct bdev_unit
{
  char path[PATH_MAX + 1];    // Pathname of this mount point
  int portid;                 // msgport id of this mount point

  struct stat stat;           // stat structure passed to mount()
  
  block64_t start;            // start block
  off64_t size;               // size in bytes
  block64_t blocks;           // number of 512 byte blocks  
};


// @brief   On-disk partition table entry in the Master Boot Record
struct mbr_partition_table_entry
{
	uint8_t state;			
	uint8_t start_head;
	uint16_t start_cylsec;
	uint8_t type;
	uint8_t end_head;
	uint16_t end_cylsec;
	uint32_t start_lba;
	uint32_t size;			/* In sectors? */
} __attribute__ (( __packed__ ));


// Configuration settings of the sdcard device driver
struct Config
{
  char pathname[PATH_MAX + 1];
  uid_t uid;
  gid_t gid;  
  mode_t mode;
  dev_t dev;
};


// @brief   structure representing the SD Card device.
struct block_device
{
  char *driver_name;    // used ?
  char *device_name;    // used ?
  uint8_t *device_id;   // used ?
  size_t dev_id_len;    // used ?

  int supports_multiple_block_read;
  int supports_multiple_block_write;

  int (*read)(struct block_device *dev, uint8_t *buf, size_t buf_size,
              uint32_t block_num);
  int (*write)(struct block_device *dev, uint8_t *buf, size_t buf_size,
               uint32_t block_num);

  size_t block_size;    // used ?
  off64_t num_blocks;   // used ?
};


// emmc.c
int sd_card_init(struct block_device **dev);
int sd_read(struct block_device *dev, uint8_t *buf, size_t buf_size, uint32_t block_no);
int sd_write(struct block_device *dev, uint8_t *buf, size_t buf_size, uint32_t block_no);

// init.c
void init(int argc, char *argv[]);
int process_args(int argc, char *argv[]);
int enable_power_and_clocks(void);
int map_io_registers(void);
int create_device_mount(void);
int create_partition_mounts(void);

// main.c
void sdcard_read(struct bdev_unit *unit, msgid_t msgid, struct fsreq *req);
void sdcard_write(struct bdev_unit *unit, msgid_t msgid, struct fsreq *req);

// timer.c
int delay_microsecs(int usec);

#endif

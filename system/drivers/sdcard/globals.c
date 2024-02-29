#include "globals.h"
#include "sdcard.h"
#include <sys/syscalls.h>
#include <sys/types.h>


uint32_t emmc_base;             // virtual address where the emmc registers are mapped
uint32_t mbox_base;             // virtual address where the mailbox registers are mapped (needed?)
uint32_t timer_clo;

//uint32_t mailbuffer_virt_addr;   // virtual address of the mailbox buffer
//uint32_t mailbuffer_phys_addr;

struct block_device actual_device;
struct block_device *bdev;

uint8_t bootsector[512];        // buffer to read bootsector into
uint8_t *buf;

int kq;                         // kqueue handle

struct Config config;

int nunits;                     // number of discovered units and partitions.
struct bdev_unit unit[5];      // Maximum 5 units, e.g. sda, sda1, sda2, sda3 and sda5



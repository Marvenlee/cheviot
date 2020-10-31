#include "globals.h"
#include "block.h"
#include <sys/syscalls.h>
#include <sys/types.h>

uint32_t mb_addr;
uint32_t emmc_base;

uint32_t timer_clo;
uint32_t mbox_base;

struct block_device actual_device;

struct block_device *bdev;

// uint8 bootsector[512];

volatile uint32 *gpio;

uint8_t *buf;
int fd;
int connection_socket;

struct Config config;


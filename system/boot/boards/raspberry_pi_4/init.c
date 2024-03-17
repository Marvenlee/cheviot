/*
 * Copyright 2023  Marven Gilhespie
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

#include <machine/cheviot_hal.h>
#include "aux_uart.h"
#include "debug.h"
#include "gpio.h"
#include "peripheral_base.h"
#include "globals.h"

// Mailbox memory
static uint32_t mailbuffer[64] __attribute__((aligned(16)));

uint32_t revision;
uint32_t ext_revision;
uint32_t videocore_base;
uint32_t videocore_size;


// Prototypes
void init_gpios(void);
int boot_rpi_mailbox(uint32_t tag, void *request, int req_sz, void *response, int response_sz);
void mailbox_get_videocore_memory(void);
void mailbox_get_revision(void);
void parse_revision(void);


/*
 *
 */
void board_init(void)
{
  hal_set_mbox_base((void *)MBOX_BASE);

  gpio_regs = (struct bcm2711_gpio_registers *)GPIO_BASE;
  aux_regs = (struct bcm2711_aux_registers *)AUX_BASE;
  
  init_aux_uart();  
  aux_uart_write_byte('\n');  

  boot_log_info("Bootloader board_init");

	mailbox_get_revision();
	mailbox_get_videocore_memory();
	parse_revision();
	init_gpios();

  boot_log_info("gpios initialized");
}


/* 
 *
 */
void init_gpios(void)
{
	uint32_t val;

	// Control mapping of EMMC/SDIO interface pins.  Thi
	// This is controlled by bit 1 of 0x7e2000d0 - 0=EMMC2, 1=legacy EMMC.	
	val = hal_mmio_read((void *)0x7e2000d0);
	val &= ~(1<<1);
	hal_mmio_write((void *)0x7e2000d0, val);

	// Disable the 1.8V power, by writing a 0 to the external (I2C expander?) GPIO pin 4
	// using the property mailbox function 0x00038041.
	// FIXME: Unsure if this is true, see sd_io_1v8_reg in Linux devicetree.	
	//	init_sdcard_1v8_reg();

  // Wi-Fi
  configure_gpio(34, INPUT, PULL_NONE);
  configure_gpio(35, INPUT, PULL_NONE);
  configure_gpio(36, INPUT, PULL_NONE);
  configure_gpio(37, INPUT, PULL_NONE);
  configure_gpio(38, INPUT, PULL_NONE);
  configure_gpio(39, INPUT, PULL_NONE);

	// SD-Card
  configure_gpio(48, FN3, PULL_NONE);
  configure_gpio(49, FN3, PULL_NONE);
  configure_gpio(50, FN3, PULL_NONE);
  configure_gpio(51, FN3, PULL_NONE);
  configure_gpio(52, FN3, PULL_NONE);
  configure_gpio(44, FN3, PULL_NONE);	
}


/*
 *
 */
int boot_rpi_mailbox(uint32_t tag, void *request, int req_sz, void *response, int response_sz)
{
  uint32_t result;
	uint32_t clock_id;
	uint32_t freq;
	uint32_t actual_response_sz;
	
	memset(mailbuffer, 0, sizeof mailbuffer);
		
  mailbuffer[0] = 64 * 4;				// max_buffer size
  mailbuffer[1] = 0;						// request code (0)
  mailbuffer[2] = tag;   				// tag id / command
  mailbuffer[3] = req_sz;				// value buffer size in bytes (4 bytes)
  mailbuffer[4] = 0;            // request/response code (request)

	if (request != NULL && req_sz > 0)
  {
  	memcpy(&mailbuffer[5], request, req_sz);
  }
  
  mailbuffer[5 + req_sz/4] = 0;		// end tag

  do {
    hal_mbox_write(MBOX_PROP, mailbuffer);
    result = hal_mbox_read(MBOX_PROP);
  } while (result == 0);

	// TODO: Add additional checks of response

	actual_response_sz = mailbuffer[3];	
	if (actual_response_sz > response_sz) {
		boot_log_info("mailbox response too long");
		return -1;
	}

	boot_log_info("actual_response_sz:%d", actual_response_sz);

	if (response != NULL) {	
		memcpy(response, &mailbuffer[5], actual_response_sz);
	}
	return 0;
}


/*
 *
 */
void mailbox_get_videocore_memory(void)
{
	uint32_t request[2] = {0};
	uint32_t response[2] = {0};
	
	boot_rpi_mailbox(0x00010006, request, sizeof request, &response, sizeof response);
	
  videocore_base = response[0];
  videocore_size = response[1];
  
	boot_log_info("videocore_base: %08x", response[0]);  
	boot_log_info("videocore_size: %08x", response[1]);  
}


/*
 *
 */
void mailbox_get_revision(void)
{
  uint32_t request[2] = {0};
  uint32_t response[2] = {0};
	
	boot_rpi_mailbox(0x00010002, request, sizeof request, &response, sizeof response);
	
  revision = response[0];
  ext_revision = response[1];
  
	boot_log_info("revision: %08x", revision);  
	boot_log_info("ext_revision: %08x", ext_revision);  
}


/*
 *
 */
void parse_revision(void)
{
	uint32_t board_type;
	uint32_t memory_config;
	uint32_t cpu_type;
		
	boot_log_info("revision: %08x", revision);

	bootinfo.revision = revision;
	
	if ((revision & (1<<23)) == 0) {
		boot_log_info("Old style revision code");		
		bootinfo.mem_size = 128 * 1024 * 1024;		// Assume 128MB
		bootinfo.board_type = 0xFF;
		bootinfo.cpu_type = 0x0F;
		strcpy(bootinfo.cpustr, "UNKNOWN");
		return;
	}
	
	board_type = (revision >> 4) & 0x00FF;
	bootinfo.board_type = board_type;
	
	boot_log_info("board_type = %02x", board_type);	
	
	memory_config = (revision >> 20) & 0x07;
	
	switch (memory_config) {
		case 0:  // 256mb
			bootinfo.mem_size = 256 * 1024 * 1024;
			break;
		case 1:  // 512mb
			bootinfo.mem_size = 512 * 1024 * 1024;
			break;
		case 2:  // 1gb
			bootinfo.mem_size = 1024 * 1024 * 1024;
			break;
		case 3:  // 2gb
			bootinfo.mem_size = 2048 * 1024 * 1024;
			break;
		case 4:  // 4gb
			bootinfo.mem_size = 4096 * 1024 * 1024;
			break;
		case 5:  // 8gb
			bootinfo.mem_size = 8192 * 1024 * 1024;
			break;
		default:
			boot_log_info("unknown memory size, assuming 128MB");
			bootinfo.mem_size = 128 * 1024 * 1024;
			break;
	}
	
	boot_log_info("System Memory: %u", (uint32_t)bootinfo.mem_size / (1024 * 1024));
	
	if (bootinfo.mem_size >= 0x7FF00000) {
		boot_log_info("Limiting system memory to 2GB (-1MB)");
		bootinfo.mem_size = 0x7FF00000;
	}
	
	cpu_type = (revision >> 12) & 0x0F;
	
	switch (cpu_type) {
		case 0:
			strcpy(bootinfo.cpustr, "BCM2835");
			break;
		case 1:
			strcpy(bootinfo.cpustr, "BCM2836");
			break;
		case 2:
			strcpy(bootinfo.cpustr, "BCM2837");
			break;
		case 3:
			strcpy(bootinfo.cpustr, "BCM2711");
			break;
		case 4:
			strcpy(bootinfo.cpustr, "BCM2712");		
			break;
		default:
			strcpy(bootinfo.cpustr, "UNKNOWN");
			break;
	}
	
	boot_log_info("cpu type: %s", bootinfo.cpustr);
	
	bootinfo.videocore_base = videocore_base;
	bootinfo.videocore_ceiling = videocore_base + videocore_size;
	
	boot_log_info("videocore_base: %08x", bootinfo.videocore_base);
	boot_log_info("videocore_ceil: %08x", bootinfo.videocore_ceiling);	
}



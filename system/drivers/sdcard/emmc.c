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

/* Provides an interface to the EMMC controller and commands for interacting
 * with an sd card */

/* References:
 *
 * PLSS 	- SD Group Physical Layer Simplified Specification ver 3.00
 * HCSS		- SD Group Host Controller Simplified Specification ver 3.00
 *
 * Broadcom BCM2835 Peripherals Guide
 */

//#define NDEBUG
//#define EMMC_DEBUG
#define LOG_LEVEL_WARN

#include "sdcard.h"
#include "mmio.h"
#include "globals.h"
#include <machine/cheviot_hal.h>
#include <time.h>
#include "timer.h"
#include "util.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/debug.h>
#include <sys/syscalls.h>
#include <unistd.h>

/*
 * Configuration options
 */
 
// Enable 1.8V support
// #define SD_1_8V_SUPPORT

// Enable 4-bit support
// #define SD_4BIT_DATA

// SD Clock Frequencies (in Hz)
#define SD_CLOCK_ID 400000
#define SD_CLOCK_NORMAL 25000000
#define SD_CLOCK_HIGH 50000000
#define SD_CLOCK_100 100000000
#define SD_CLOCK_208 208000000

// Enable SDXC maximum performance mode
// Requires 150 mA power so disabled on the RPi for now
//#define SDXC_MAXIMUM_PERFORMANCE

// Enable SDMA support
//#define SDMA_SUPPORT

// SDMA buffer address
#define SDMA_BUFFER 0x6000
#define SDMA_BUFFER_PA (SDMA_BUFFER + 0xC0000000)

// Enable card interrupts
//#define SD_CARD_INTERRUPTS

// Enable EXPERIMENTAL (and possibly DANGEROUS) SD write support
#define SD_WRITE_SUPPORT   1

// The particular SDHCI implementation
#define SDHCI_IMPLEMENTATION_GENERIC    0
#define SDHCI_IMPLEMENTATION_BCM_2708   1
#define SDHCI_IMPLEMENTATION            SDHCI_IMPLEMENTATION_GENERIC

static char driver_name[] = "emmc";
static char device_name[] =
    "emmc0"; // We use a single device name as there is only
// one card slot in the RPi

static uint32_t hci_ver = 0;
static uint32_t capabilities_0 = 0;
static uint32_t capabilities_1 = 0;

struct sd_scr {
  uint32_t scr[2];
  uint32_t sd_bus_widths;
  int sd_version;
};

struct emmc_block_dev {
  struct block_device bd;
  uint32_t card_supports_sdhc;
  uint32_t card_supports_18v;
  uint32_t card_ocr;
  uint32_t card_rca;
  uint32_t last_interrupt;
  uint32_t last_error;

  struct sd_scr *scr;

  int failed_voltage_switch;

  uint32_t last_cmd_reg;
  uint32_t last_cmd;
  uint32_t last_cmd_success;
  uint32_t last_r0;
  uint32_t last_r1;
  uint32_t last_r2;
  uint32_t last_r3;

  void *buf;
  int blocks_to_transfer;
  size_t block_size;
  int use_sdma;
  int card_removal;
  uint32_t base_clock;
};

#define EMMC_ARG2 0
#define EMMC_BLKSIZECNT 4
#define EMMC_ARG1 8
#define EMMC_CMDTM 0xC
#define EMMC_RESP0 0x10
#define EMMC_RESP1 0x14
#define EMMC_RESP2 0x18
#define EMMC_RESP3 0x1C
#define EMMC_DATA 0x20
#define EMMC_STATUS 0x24
#define EMMC_CONTROL0 0x28
#define EMMC_CONTROL1 0x2C
#define EMMC_INTERRUPT 0x30
#define EMMC_IRPT_MASK 0x34
#define EMMC_IRPT_EN 0x38
#define EMMC_CONTROL2 0x3C
#define EMMC_CAPABILITIES_0 0x40
#define EMMC_CAPABILITIES_1 0x44
#define EMMC_FORCE_IRPT 0x50
#define EMMC_BOOT_TIMEOUT 0x70
#define EMMC_DBG_SEL 0x74
#define EMMC_EXRDFIFO_CFG 0x80
#define EMMC_EXRDFIFO_EN 0x84
#define EMMC_TUNE_STEP 0x88
#define EMMC_TUNE_STEPS_STD 0x8C
#define EMMC_TUNE_STEPS_DDR 0x90
#define EMMC_SPI_INT_SPT 0xF0
#define EMMC_SLOTISR_VER 0xFC

#define SD_CMD_INDEX(a) ((a) << 24)
#define SD_CMD_TYPE_NORMAL 0x0
#define SD_CMD_TYPE_SUSPEND (1 << 22)
#define SD_CMD_TYPE_RESUME (2 << 22)
#define SD_CMD_TYPE_ABORT (3 << 22)
#define SD_CMD_TYPE_MASK (3 << 22)
#define SD_CMD_ISDATA (1 << 21)
#define SD_CMD_IXCHK_EN (1 << 20)
#define SD_CMD_CRCCHK_EN (1 << 19)
#define SD_CMD_RSPNS_TYPE_NONE 0 // For no response
#define SD_CMD_RSPNS_TYPE_136                                                  \
  (1 << 16) // For response R2 (with CRC), R3,4 (no CRC)
#define SD_CMD_RSPNS_TYPE_48                                                   \
  (2 << 16) // For responses R1, R5, R6, R7 (with CRC)
#define SD_CMD_RSPNS_TYPE_48B (3 << 16) // For responses R1b, R5b (with CRC)
#define SD_CMD_RSPNS_TYPE_MASK (3 << 16)
#define SD_CMD_MULTI_BLOCK (1 << 5)
#define SD_CMD_DAT_DIR_HC 0
#define SD_CMD_DAT_DIR_CH (1 << 4)
#define SD_CMD_AUTO_CMD_EN_NONE 0
#define SD_CMD_AUTO_CMD_EN_CMD12 (1 << 2)
#define SD_CMD_AUTO_CMD_EN_CMD23 (2 << 2)
#define SD_CMD_BLKCNT_EN (1 << 1)
#define SD_CMD_DMA 1

#define SD_ERR_CMD_TIMEOUT 0
#define SD_ERR_CMD_CRC 1
#define SD_ERR_CMD_END_BIT 2
#define SD_ERR_CMD_INDEX 3
#define SD_ERR_DATA_TIMEOUT 4
#define SD_ERR_DATA_CRC 5
#define SD_ERR_DATA_END_BIT 6
#define SD_ERR_CURRENT_LIMIT 7
#define SD_ERR_AUTO_CMD12 8
#define SD_ERR_ADMA 9
#define SD_ERR_TUNING 10
#define SD_ERR_RSVD 11

#define SD_ERR_MASK_CMD_TIMEOUT (1 << (16 + SD_ERR_CMD_TIMEOUT))
#define SD_ERR_MASK_CMD_CRC (1 << (16 + SD_ERR_CMD_CRC))
#define SD_ERR_MASK_CMD_END_BIT (1 << (16 + SD_ERR_CMD_END_BIT))
#define SD_ERR_MASK_CMD_INDEX (1 << (16 + SD_ERR_CMD_INDEX))
#define SD_ERR_MASK_DATA_TIMEOUT (1 << (16 + SD_ERR_CMD_TIMEOUT))
#define SD_ERR_MASK_DATA_CRC (1 << (16 + SD_ERR_CMD_CRC))
#define SD_ERR_MASK_DATA_END_BIT (1 << (16 + SD_ERR_CMD_END_BIT))
#define SD_ERR_MASK_CURRENT_LIMIT (1 << (16 + SD_ERR_CMD_CURRENT_LIMIT))
#define SD_ERR_MASK_AUTO_CMD12 (1 << (16 + SD_ERR_CMD_AUTO_CMD12))
#define SD_ERR_MASK_ADMA (1 << (16 + SD_ERR_CMD_ADMA))
#define SD_ERR_MASK_TUNING (1 << (16 + SD_ERR_CMD_TUNING))

#define SD_COMMAND_COMPLETE 1
#define SD_TRANSFER_COMPLETE (1 << 1)
#define SD_BLOCK_GAP_EVENT (1 << 2)
#define SD_DMA_INTERRUPT (1 << 3)
#define SD_BUFFER_WRITE_READY (1 << 4)
#define SD_BUFFER_READ_READY (1 << 5)
#define SD_CARD_INSERTION (1 << 6)
#define SD_CARD_REMOVAL (1 << 7)
#define SD_CARD_INTERRUPT (1 << 8)

#define SD_RESP_NONE SD_CMD_RSPNS_TYPE_NONE
#define SD_RESP_R1 (SD_CMD_RSPNS_TYPE_48 | SD_CMD_CRCCHK_EN)
#define SD_RESP_R1b (SD_CMD_RSPNS_TYPE_48B | SD_CMD_CRCCHK_EN)
#define SD_RESP_R2 (SD_CMD_RSPNS_TYPE_136 | SD_CMD_CRCCHK_EN)
#define SD_RESP_R3 SD_CMD_RSPNS_TYPE_48
#define SD_RESP_R4 SD_CMD_RSPNS_TYPE_136
#define SD_RESP_R5 (SD_CMD_RSPNS_TYPE_48 | SD_CMD_CRCCHK_EN)
#define SD_RESP_R5b (SD_CMD_RSPNS_TYPE_48B | SD_CMD_CRCCHK_EN)
#define SD_RESP_R6 (SD_CMD_RSPNS_TYPE_48 | SD_CMD_CRCCHK_EN)
#define SD_RESP_R7 (SD_CMD_RSPNS_TYPE_48 | SD_CMD_CRCCHK_EN)

#define SD_DATA_READ (SD_CMD_ISDATA | SD_CMD_DAT_DIR_CH)
#define SD_DATA_WRITE (SD_CMD_ISDATA | SD_CMD_DAT_DIR_HC)

#define SD_CMD_RESERVED(a) 0xffffffff

#define SUCCESS(a) (a->last_cmd_success)
#define FAIL(a) (a->last_cmd_success == 0)
#define TIMEOUT(a) (FAIL(a) && (a->last_error == 0))
#define CMD_TIMEOUT(a) (FAIL(a) && (a->last_error & (1 << 16)))
#define CMD_CRC(a) (FAIL(a) && (a->last_error & (1 << 17)))
#define CMD_END_BIT(a) (FAIL(a) && (a->last_error & (1 << 18)))
#define CMD_INDEX(a) (FAIL(a) && (a->last_error & (1 << 19)))
#define DATA_TIMEOUT(a) (FAIL(a) && (a->last_error & (1 << 20)))
#define DATA_CRC(a) (FAIL(a) && (a->last_error & (1 << 21)))
#define DATA_END_BIT(a) (FAIL(a) && (a->last_error & (1 << 22)))
#define CURRENT_LIMIT(a) (FAIL(a) && (a->last_error & (1 << 23)))
#define ACMD12_ERROR(a) (FAIL(a) && (a->last_error & (1 << 24)))
#define ADMA_ERROR(a) (FAIL(a) && (a->last_error & (1 << 25)))
#define TUNING_ERROR(a) (FAIL(a) && (a->last_error & (1 << 26)))

#define SD_VER_UNKNOWN 0
#define SD_VER_1 1
#define SD_VER_1_1 2
#define SD_VER_2 3
#define SD_VER_3 4
#define SD_VER_4 5

static char *sd_versions[] = {"unknown", "1.0 and 1.01", "1.10",
                              "2.00",    "3.0x",         "4.xx"};

#ifdef EMMC_DEBUG
static char *err_irpts[] = {"CMD_TIMEOUT",  "CMD_CRC",       "CMD_END_BIT",
                            "CMD_INDEX",    "DATA_TIMEOUT",  "DATA_CRC",
                            "DATA_END_BIT", "CURRENT_LIMIT", "AUTO_CMD12",
                            "ADMA",         "TUNING",        "RSVD"};
#endif

int sd_read(struct block_device *, uint8_t *, size_t buf_size, uint32_t);
int sd_write(struct block_device *, uint8_t *, size_t buf_size, uint32_t);

static uint32_t sd_commands[] = {
    SD_CMD_INDEX(0), SD_CMD_RESERVED(1), SD_CMD_INDEX(2) | SD_RESP_R2,
    SD_CMD_INDEX(3) | SD_RESP_R6, SD_CMD_INDEX(4), SD_CMD_INDEX(5) | SD_RESP_R4,
    SD_CMD_INDEX(6) | SD_RESP_R1, SD_CMD_INDEX(7) | SD_RESP_R1b,
    SD_CMD_INDEX(8) | SD_RESP_R7, SD_CMD_INDEX(9) | SD_RESP_R2,
    SD_CMD_INDEX(10) | SD_RESP_R2, SD_CMD_INDEX(11) | SD_RESP_R1,
    SD_CMD_INDEX(12) | SD_RESP_R1b | SD_CMD_TYPE_ABORT,
    SD_CMD_INDEX(13) | SD_RESP_R1, SD_CMD_RESERVED(14), SD_CMD_INDEX(15),
    SD_CMD_INDEX(16) | SD_RESP_R1, SD_CMD_INDEX(17) | SD_RESP_R1 | SD_DATA_READ,
    SD_CMD_INDEX(18) | SD_RESP_R1 | SD_DATA_READ | SD_CMD_MULTI_BLOCK |
        SD_CMD_BLKCNT_EN,
    SD_CMD_INDEX(19) | SD_RESP_R1 | SD_DATA_READ,
    SD_CMD_INDEX(20) | SD_RESP_R1b, SD_CMD_RESERVED(21), SD_CMD_RESERVED(22),
    SD_CMD_INDEX(23) | SD_RESP_R1,
    SD_CMD_INDEX(24) | SD_RESP_R1 | SD_DATA_WRITE,
    SD_CMD_INDEX(25) | SD_RESP_R1 | SD_DATA_WRITE | SD_CMD_MULTI_BLOCK |
        SD_CMD_BLKCNT_EN,
    SD_CMD_RESERVED(26), SD_CMD_INDEX(27) | SD_RESP_R1 | SD_DATA_WRITE,
    SD_CMD_INDEX(28) | SD_RESP_R1b, SD_CMD_INDEX(29) | SD_RESP_R1b,
    SD_CMD_INDEX(30) | SD_RESP_R1 | SD_DATA_READ, SD_CMD_RESERVED(31),
    SD_CMD_INDEX(32) | SD_RESP_R1, SD_CMD_INDEX(33) | SD_RESP_R1,
    SD_CMD_RESERVED(34), SD_CMD_RESERVED(35), SD_CMD_RESERVED(36),
    SD_CMD_RESERVED(37), SD_CMD_INDEX(38) | SD_RESP_R1b, SD_CMD_RESERVED(39),
    SD_CMD_RESERVED(40), SD_CMD_RESERVED(41), SD_CMD_RESERVED(42) | SD_RESP_R1,
    SD_CMD_RESERVED(43), SD_CMD_RESERVED(44), SD_CMD_RESERVED(45),
    SD_CMD_RESERVED(46), SD_CMD_RESERVED(47), SD_CMD_RESERVED(48),
    SD_CMD_RESERVED(49), SD_CMD_RESERVED(50), SD_CMD_RESERVED(51),
    SD_CMD_RESERVED(52), SD_CMD_RESERVED(53), SD_CMD_RESERVED(54),
    SD_CMD_INDEX(55) | SD_RESP_R1,
    SD_CMD_INDEX(56) | SD_RESP_R1 | SD_CMD_ISDATA, SD_CMD_RESERVED(57),
    SD_CMD_RESERVED(58), SD_CMD_RESERVED(59), SD_CMD_RESERVED(60),
    SD_CMD_RESERVED(61), SD_CMD_RESERVED(62), SD_CMD_RESERVED(63)};

static uint32_t sd_acommands[] = {SD_CMD_RESERVED(0),
                                  SD_CMD_RESERVED(1),
                                  SD_CMD_RESERVED(2),
                                  SD_CMD_RESERVED(3),
                                  SD_CMD_RESERVED(4),
                                  SD_CMD_RESERVED(5),
                                  SD_CMD_INDEX(6) | SD_RESP_R1,
                                  SD_CMD_RESERVED(7),
                                  SD_CMD_RESERVED(8),
                                  SD_CMD_RESERVED(9),
                                  SD_CMD_RESERVED(10),
                                  SD_CMD_RESERVED(11),
                                  SD_CMD_RESERVED(12),
                                  SD_CMD_INDEX(13) | SD_RESP_R1,
                                  SD_CMD_RESERVED(14),
                                  SD_CMD_RESERVED(15),
                                  SD_CMD_RESERVED(16),
                                  SD_CMD_RESERVED(17),
                                  SD_CMD_RESERVED(18),
                                  SD_CMD_RESERVED(19),
                                  SD_CMD_RESERVED(20),
                                  SD_CMD_RESERVED(21),
                                  SD_CMD_INDEX(22) | SD_RESP_R1 | SD_DATA_READ,
                                  SD_CMD_INDEX(23) | SD_RESP_R1,
                                  SD_CMD_RESERVED(24),
                                  SD_CMD_RESERVED(25),
                                  SD_CMD_RESERVED(26),
                                  SD_CMD_RESERVED(27),
                                  SD_CMD_RESERVED(28),
                                  SD_CMD_RESERVED(29),
                                  SD_CMD_RESERVED(30),
                                  SD_CMD_RESERVED(31),
                                  SD_CMD_RESERVED(32),
                                  SD_CMD_RESERVED(33),
                                  SD_CMD_RESERVED(34),
                                  SD_CMD_RESERVED(35),
                                  SD_CMD_RESERVED(36),
                                  SD_CMD_RESERVED(37),
                                  SD_CMD_RESERVED(38),
                                  SD_CMD_RESERVED(39),
                                  SD_CMD_RESERVED(40),
                                  SD_CMD_INDEX(41) | SD_RESP_R3,
                                  SD_CMD_INDEX(42) | SD_RESP_R1,
                                  SD_CMD_RESERVED(43),
                                  SD_CMD_RESERVED(44),
                                  SD_CMD_RESERVED(45),
                                  SD_CMD_RESERVED(46),
                                  SD_CMD_RESERVED(47),
                                  SD_CMD_RESERVED(48),
                                  SD_CMD_RESERVED(49),
                                  SD_CMD_RESERVED(50),
                                  SD_CMD_INDEX(51) | SD_RESP_R1 | SD_DATA_READ,
                                  SD_CMD_RESERVED(52),
                                  SD_CMD_RESERVED(53),
                                  SD_CMD_RESERVED(54),
                                  SD_CMD_RESERVED(55),
                                  SD_CMD_RESERVED(56),
                                  SD_CMD_RESERVED(57),
                                  SD_CMD_RESERVED(58),
                                  SD_CMD_RESERVED(59),
                                  SD_CMD_RESERVED(60),
                                  SD_CMD_RESERVED(61),
                                  SD_CMD_RESERVED(62),
                                  SD_CMD_RESERVED(63)};

// The actual command indices
#define GO_IDLE_STATE 0
#define ALL_SEND_CID 2
#define SEND_RELATIVE_ADDR 3
#define SET_DSR 4
#define IO_SET_OP_COND 5
#define SWITCH_FUNC 6
#define SELECT_CARD 7
#define DESELECT_CARD 7
#define SELECT_DESELECT_CARD 7
#define SEND_IF_COND 8
#define SEND_CSD 9
#define SEND_CID 10
#define VOLTAGE_SWITCH 11
#define STOP_TRANSMISSION 12
#define SEND_STATUS 13
#define GO_INACTIVE_STATE 15
#define SET_BLOCKLEN 16
#define READ_SINGLE_BLOCK 17
#define READ_MULTIPLE_BLOCK 18
#define SEND_TUNING_BLOCK 19
#define SPEED_CLASS_CONTROL 20
#define SET_BLOCK_COUNT 23
#define WRITE_BLOCK 24
#define WRITE_MULTIPLE_BLOCK 25
#define PROGRAM_CSD 27
#define SET_WRITE_PROT 28
#define CLR_WRITE_PROT 29
#define SEND_WRITE_PROT 30
#define ERASE_WR_BLK_START 32
#define ERASE_WR_BLK_END 33
#define ERASE 38
#define LOCK_UNLOCK 42
#define APP_CMD 55
#define GEN_CMD 56

#define IS_APP_CMD 0x80000000
#define ACMD(a) (a | IS_APP_CMD)
#define SET_BUS_WIDTH (6 | IS_APP_CMD)
#define SD_STATUS (13 | IS_APP_CMD)
#define SEND_NUM_WR_BLOCKS (22 | IS_APP_CMD)
#define SET_WR_BLK_ERASE_COUNT (23 | IS_APP_CMD)
#define SD_SEND_OP_COND (41 | IS_APP_CMD)
#define SET_CLR_CARD_DETECT (42 | IS_APP_CMD)
#define SEND_SCR (51 | IS_APP_CMD)

#define SD_RESET_CMD (1 << 25)
#define SD_RESET_DAT (1 << 26)
#define SD_RESET_ALL (1 << 24)

#define SD_GET_CLOCK_DIVIDER_FAIL 0xffffffff


/* @brief   Power off the SD card 
 *
 */
static void sd_power_off() {
  uint32_t control0 = mmio_read(emmc_base + EMMC_CONTROL0);
  control0 &= ~(1 << 8); // Set SD Bus Power bit off in Power Control Register
  mmio_write(emmc_base + EMMC_CONTROL0, control0);
}


/* @brief   Get the base clock rate
 *
 */
static uint32_t sd_get_base_clock_hz() {
  uint32_t base_clock;
  
#if BASE_CLOCK_SRC == EMMC_CAPABILITIES
  capabilities_0 = mmio_read(emmc_base + EMMC_CAPABILITIES_0);
  base_clock = ((capabilities_0 >> 8) & 0xff) * 1000000;
#elif

#elif BASE_CLOCK_SRC == RPI_MAILBOX
	rpi_mbox_get_clock_rate(MBOX_CLOCK_ID_EMMC2, &base_clock);	
#else
  log_warn("get_base_clock_hz() is not implemented for this architecture.");
  return 0;
#endif

  log_info("base clock rate is %i Hz", base_clock);
  return base_clock;
}


#if SDHCI_IMPLEMENTATION == SDHCI_IMPLEMENTATION_BCM_2708

/* @brief   Power off the SD Card
 *
 */
static int bcm_2708_power_off()
{
  volatile uint32_t *mailbuffer = (uint32_t *)mailbuffer_virt_addr;

  // set up the buffer
  mailbuffer[0] = 8 * 4; // size of this message
  mailbuffer[1] = 0;     // this is a request

  // next comes the first tag
  mailbuffer[2] = 0x00028001; // set power state tag
  mailbuffer[3] = 0x8;        // value buffer size
  mailbuffer[4] = 0x8;        // is a request, value length = 8
  mailbuffer[5] = 0x0;        // device id and device id also returned here
  mailbuffer[6] = 0x2; 				// set power off, wait for stable and returns state
  mailbuffer[7] = 0;					// closing tag

  // send the message
  mbox_write(MBOX_PROP, mailbuffer_phys_addr);

  // read the response
  mbox_read(MBOX_PROP);

  if (mailbuffer[1] != MBOX_SUCCESS) {
    log_error("bcm_2708_power_off(): property mailbox did not return a valid "
         "response.");
    return -1;
  }

  if (mailbuffer[5] != 0x0) {
    log_info("property mailbox did not return a valid device id.");
    return -1;
  }

  if ((mailbuffer[6] & 0x3) != 0) {
    log_info("bcm_2708_power_off(): device did not power off successfully (%08x).", mailbuffer[6]);

    for (int t = 0; t < 8; t++) {
      log_info("mb[%d] = %08x", t, mailbuffer[t]);
    }

    return 1;
  }

  return 0;
}


/* @brief   Power on the SD card
 *
 */
static int bcm_2708_power_on() {
  volatile uint32_t *mailbuffer = (uint32_t *)mailbuffer_virt_addr;

  // set up the buffer
  mailbuffer[0] = 8 * 4; // size of this message
  mailbuffer[1] = 0;     // this is a request

  // next comes the first tag
  mailbuffer[2] = 0x00028001; // set power state tag
  mailbuffer[3] = 0x8;        // value buffer size
  mailbuffer[4] = 0x8;        // is a request, value length = 8
  mailbuffer[5] = 0x0;        // device id and device id also returned here
  mailbuffer[6] = 0x3; 				// set power on, wait for stable and returns state
  mailbuffer[7] = 0;					// closing tag

  // send the message
  mbox_write(MBOX_PROP, mailbuffer_phys_addr);

  // read the response
  mbox_read(MBOX_PROP);

  if (mailbuffer[1] != MBOX_SUCCESS) {
    log_error("bcm_2708_power_on(): property mailbox did not return a valid response.");
    return -1;
  }

  if (mailbuffer[5] != 0x0) {
    log_error("property mailbox did not return a valid device id.");
    return -1;
  }

  if ((mailbuffer[6] & 0x3) != 1) {
    log_info("bcm_2708_power_on(): device did not power on successfully (%08x).", mailbuffer[6]);
    return 1;
  }

  return 0;
}


/* @brief   Power cycle the SDIO interface
 *
 */
int bcm_2708_power_cycle() {
  if (bcm_2708_power_off() < 0)
    return -1;

  delay_microsecs(5000);

  return bcm_2708_power_on();
}

#endif // SDHCI_IMPLEMENTATION == SDHCI_IMPLEMENTATION_BCM_2708


/* @brief   Set the clock dividers to generate a target value
 *
 */
static uint32_t sd_get_clock_divider(uint32_t base_clock,
                                     uint32_t target_rate) {
  // TODO: implement use of preset value registers

  uint32_t targetted_divisor = 0;
  if (target_rate > base_clock)
    targetted_divisor = 1;
  else {
    targetted_divisor = base_clock / target_rate;
    uint32_t mod = base_clock % target_rate;
    if (mod)
      targetted_divisor--;
  }

  // Decide on the clock mode to use

  // Currently only 10-bit divided clock mode is supported

  if (hci_ver >= 2) {
    // HCI version 3 or greater supports 10-bit divided clock mode
    // This requires a power-of-two divider

    // Find the first bit set
    int divisor = -1;
    for (int first_bit = 31; first_bit >= 0; first_bit--) {
      uint32_t bit_test = (1 << first_bit);
      if (targetted_divisor & bit_test) {
        divisor = first_bit;
        targetted_divisor &= ~bit_test;
        if (targetted_divisor) {
          // The divisor is not a power-of-two, increase it
          divisor++;
        }
        break;
      }
    }

    if (divisor == -1)
      divisor = 31;
    if (divisor >= 32)
      divisor = 31;

    if (divisor != 0)
      divisor = (1 << (divisor - 1));

    if (divisor >= 0x400)
      divisor = 0x3ff;

    uint32_t freq_select = divisor & 0xff;
    uint32_t upper_bits = (divisor >> 8) & 0x3;
    uint32_t ret = (freq_select << 8) | (upper_bits << 6) | (0 << 5);

#ifdef EMMC_DEBUG
    int denominator = 1;
    if (divisor != 0)
      denominator = divisor * 2;
    int actual_clock = base_clock / denominator;
    log_info("base_clock: %i, target_rate: %i", base_clock, target_rate);
    log_info("divisor: %08x, actual_clock: %i, ret: %08x",
         						divisor, actual_clock, ret);
#endif

    return ret;
  } else {
    log_error("unsupported host version");
    return SD_GET_CLOCK_DIVIDER_FAIL;
  }
}


/* @brief   Switch the clock rate whilst running
 *
 */
static int sd_switch_clock_rate(uint32_t base_clock, uint32_t target_rate) {
  // Decide on an appropriate divider
  uint32_t divider = sd_get_clock_divider(base_clock, target_rate);
  if (divider == SD_GET_CLOCK_DIVIDER_FAIL) {
    log_error("couldn't get a valid divider for target rate %i Hz",
         target_rate);
    return -1;
  }

  // Wait for the command inhibit (CMD and DAT) bits to clear
  while (mmio_read(emmc_base + EMMC_STATUS) & 0x3)
    delay_microsecs(1000);

  // Set the SD clock off
  uint32_t control1 = mmio_read(emmc_base + EMMC_CONTROL1);
  control1 &= ~(1 << 2);
  mmio_write(emmc_base + EMMC_CONTROL1, control1);
  delay_microsecs(2000);

  // Write the new divider
  control1 &= ~0xffe0; // Clear old setting + clock generator select
  control1 |= divider;
  mmio_write(emmc_base + EMMC_CONTROL1, control1);
  delay_microsecs(2000);

  // Enable the SD clock
  control1 |= (1 << 2);
  mmio_write(emmc_base + EMMC_CONTROL1, control1);
  delay_microsecs(2000);

  log_info("successfully set clock rate to %i Hz", target_rate);
  return 0;
}


/* @brief   Reset the CMD line
 *
 */
static int sd_reset_cmd() {
  uint32_t control1 = mmio_read(emmc_base + EMMC_CONTROL1);
  control1 |= SD_RESET_CMD;
  mmio_write(emmc_base + EMMC_CONTROL1, control1);
  TIMEOUT_WAIT((mmio_read(emmc_base + EMMC_CONTROL1) & SD_RESET_CMD) == 0,
               1000000);
  if ((mmio_read(emmc_base + EMMC_CONTROL1) & SD_RESET_CMD) != 0) {
    log_error("CMD line did not reset properly");
    return -1;
  }
  return 0;
}


/* @brief   Reset the DAT line
 *
 */
static int sd_reset_dat() {
  uint32_t control1 = mmio_read(emmc_base + EMMC_CONTROL1);
  control1 |= SD_RESET_DAT;
  mmio_write(emmc_base + EMMC_CONTROL1, control1);
  TIMEOUT_WAIT((mmio_read(emmc_base + EMMC_CONTROL1) & SD_RESET_DAT) == 0,
               1000000);
  if ((mmio_read(emmc_base + EMMC_CONTROL1) & SD_RESET_DAT) != 0) {
    log_info("DAT line did not reset properly");
    return -1;
  }
  return 0;
}


/* @brief   Internal handling of issuing SDIO command
 *
 */
static void sd_issue_command_int(struct emmc_block_dev *dev, uint32_t cmd_reg,
                                 uint32_t argument, useconds_t timeout) {
  dev->last_cmd_reg = cmd_reg;
  dev->last_cmd_success = 0;

  // This is as per HCSS 3.7.1.1/3.7.2.2
  // Check Command Inhibit
  while (mmio_read(emmc_base + EMMC_STATUS) & 0x1)
    delay_microsecs(1000);

  // Is the command with busy?
  if ((cmd_reg & SD_CMD_RSPNS_TYPE_MASK) == SD_CMD_RSPNS_TYPE_48B) {
    // With busy

    // Is is an abort command?
    if ((cmd_reg & SD_CMD_TYPE_MASK) != SD_CMD_TYPE_ABORT) {
      // Not an abort command

      // Wait for the data line to be free
      while (mmio_read(emmc_base + EMMC_STATUS) & 0x2)
        delay_microsecs(1000);
    }
  }

  // Is this a DMA transfer?
  int is_sdma = 0;
  if ((cmd_reg & SD_CMD_ISDATA) && (dev->use_sdma)) {
    log_info("performing SDMA transfer, current INTERRUPT: %08x",
         mmio_read(emmc_base + EMMC_INTERRUPT));

    is_sdma = 1;
  }

  if (is_sdma) {
  	log_info("is_sdma: setting system address register");
    // Set system address register (ARGUMENT2 in RPi)
    // We need to define a 4 kiB aligned buffer to use here
    // Then convert its virtual address to a bus address
    mmio_write(emmc_base + EMMC_ARG2, SDMA_BUFFER_PA);
  }

  // Set block size and block count
  // For now, block size = 512 bytes, block count = 1,
  //  host SDMA buffer boundary = 4 kiB
  if (dev->blocks_to_transfer > 0xffff) {
    log_warn("blocks_to_transfer too great (%i)", dev->blocks_to_transfer);
    dev->last_cmd_success = 0;
    return;
  }
  
  uint32_t blksizecnt = dev->block_size | (dev->blocks_to_transfer << 16);
  mmio_write(emmc_base + EMMC_BLKSIZECNT, blksizecnt);

  // Set argument 1 reg
  mmio_write(emmc_base + EMMC_ARG1, argument);

  if (is_sdma) {
    // Set Transfer mode register
    cmd_reg |= SD_CMD_DMA;
  }

  // Set command reg
  mmio_write(emmc_base + EMMC_CMDTM, cmd_reg);

  // Wait for command complete interrupt
  TIMEOUT_WAIT(mmio_read(emmc_base + EMMC_INTERRUPT) & 0x8001, timeout);
  uint32_t irpts = mmio_read(emmc_base + EMMC_INTERRUPT);
	
  // Clear command complete status
  mmio_write(emmc_base + EMMC_INTERRUPT, 0xffff0001);

  // Test for errors
  if ((irpts & 0xffff0001) != 0x1) {
    log_error("error occured whilst waiting for command complete interrupt");
    dev->last_error = irpts & 0xffff0000;
    dev->last_interrupt = irpts;
    return;
  }
  
  // Get response data
  switch (cmd_reg & SD_CMD_RSPNS_TYPE_MASK) {
  case SD_CMD_RSPNS_TYPE_48:
  case SD_CMD_RSPNS_TYPE_48B:
    dev->last_r0 = mmio_read(emmc_base + EMMC_RESP0);
    break;

  case SD_CMD_RSPNS_TYPE_136:
    dev->last_r0 = mmio_read(emmc_base + EMMC_RESP0);
    dev->last_r1 = mmio_read(emmc_base + EMMC_RESP1);
    dev->last_r2 = mmio_read(emmc_base + EMMC_RESP2);
    dev->last_r3 = mmio_read(emmc_base + EMMC_RESP3);
    break;
  }

  // If with data, wait for the appropriate interrupt
  if ((cmd_reg & SD_CMD_ISDATA) && (is_sdma == 0)) {
    uint32_t wr_irpt;
    int is_write = 0;
    if (cmd_reg & SD_CMD_DAT_DIR_CH)
      wr_irpt = (1 << 5); // read
    else {
      is_write = 1;
      wr_irpt = (1 << 4); // write
    }

    int cur_block = 0;
    uint32_t *cur_buf_addr = (uint32_t *)dev->buf;
    while (cur_block < dev->blocks_to_transfer) {
      if (dev->blocks_to_transfer > 1) {
        log_info("multi block transfer, awaiting block %i ready", cur_block);
      }
      
      TIMEOUT_WAIT(mmio_read(emmc_base + EMMC_INTERRUPT) & (wr_irpt | 0x8000),
                   timeout);
      irpts = mmio_read(emmc_base + EMMC_INTERRUPT);
      mmio_write(emmc_base + EMMC_INTERRUPT, 0xffff0000 | wr_irpt);

      if ((irpts & (0xffff0000 | wr_irpt)) != wr_irpt) {
        log_error("error occured whilst waiting for data ready interrupt");
        dev->last_error = irpts & 0xffff0000;
        dev->last_interrupt = irpts;
        return;
      }

      // Transfer the block
      size_t cur_byte_no = 0;
      while (cur_byte_no < dev->block_size) {
        if (is_write) {
          uint32_t data = read_word((uint8_t *)cur_buf_addr, 0);
          mmio_write(emmc_base + EMMC_DATA, data);
        } else {
          uint32_t data = mmio_read(emmc_base + EMMC_DATA);
          write_word(data, (uint8_t *)cur_buf_addr, 0);
        }
        cur_byte_no += 4;
        cur_buf_addr++;
      }

      cur_block++;
    }
  }

  // Wait for transfer complete (set if read/write transfer or with busy)
  if ((((cmd_reg & SD_CMD_RSPNS_TYPE_MASK) == SD_CMD_RSPNS_TYPE_48B) ||
       (cmd_reg & SD_CMD_ISDATA)) &&
      (is_sdma == 0)) {
    // First check command inhibit (DAT) is not already 0
    if ((mmio_read(emmc_base + EMMC_STATUS) & 0x2) == 0)
      mmio_write(emmc_base + EMMC_INTERRUPT, 0xffff0002);
    else {
      TIMEOUT_WAIT(mmio_read(emmc_base + EMMC_INTERRUPT) & 0x8002, timeout);
      irpts = mmio_read(emmc_base + EMMC_INTERRUPT);
      mmio_write(emmc_base + EMMC_INTERRUPT, 0xffff0002);

      // Handle the case where both data timeout and transfer complete
      //  are set - transfer complete overrides data timeout: HCSS 2.2.17
      if (((irpts & 0xffff0002) != 0x2) && ((irpts & 0xffff0002) != 0x100002)) {
        log_error("error occured whilst waiting for transfer complete "
             "interrupt");

        dev->last_error = irpts & 0xffff0000;
        dev->last_interrupt = irpts;
        return;
      }
      mmio_write(emmc_base + EMMC_INTERRUPT, 0xffff0002);
    }
  } else if (is_sdma) {
    // For SDMA transfers, we have to wait for either transfer complete,
    //  DMA int or an error

    // First check command inhibit (DAT) is not already 0
    if ((mmio_read(emmc_base + EMMC_STATUS) & 0x2) == 0)
      mmio_write(emmc_base + EMMC_INTERRUPT, 0xffff000a);
    else {
      TIMEOUT_WAIT(mmio_read(emmc_base + EMMC_INTERRUPT) & 0x800a, timeout);
      irpts = mmio_read(emmc_base + EMMC_INTERRUPT);
      mmio_write(emmc_base + EMMC_INTERRUPT, 0xffff000a);

      // Detect errors
      if ((irpts & 0x8000) && ((irpts & 0x2) != 0x2)) {
        log_error("error occured whilst waiting for transfer complete "
             "interrupt");

        dev->last_error = irpts & 0xffff0000;
        dev->last_interrupt = irpts;
        return;
      }

      // Detect DMA interrupt without transfer complete
      // Currently not supported - all block sizes should fit in the
      //  buffer
      if ((irpts & 0x8) && ((irpts & 0x2) != 0x2)) {
        log_error("error: DMA interrupt occured without transfer complete");

        dev->last_error = irpts & 0xffff0000;
        dev->last_interrupt = irpts;
        return;
      }

      // Detect transfer complete
      if (irpts & 0x2) {
        // Transfer the data to the user buffer
        memcpy(dev->buf, (const void *)SDMA_BUFFER, dev->block_size);
      } else {
        // Unknown error
        if (irpts == 0) {
          log_error("timeout waiting for SDMA transfer to complete");
        } else {
          log_error("unknown SDMA transfer error");
        }
        
        if ((irpts == 0) && ((mmio_read(emmc_base + EMMC_STATUS) & 0x3) == 0x2)) {
          // The data transfer is ongoing, we should attempt to stop it
          log_info("aborting transfer");
          mmio_write(emmc_base + EMMC_CMDTM, sd_commands[STOP_TRANSMISSION]);
        }
        dev->last_error = irpts & 0xffff0000;
        dev->last_interrupt = irpts;
        return;
      }
    }
  }

  // Return success
  dev->last_cmd_success = 1;
}


/*
 *
 */
static void sd_handle_card_interrupt(struct emmc_block_dev *dev) {
// Handle a card interrupt

  uint32_t status = mmio_read(emmc_base + EMMC_STATUS);

//  log_info("card interrupt");
//  log_info("controller status: %08x", status);

  // Get the card status
  if (dev->card_rca) {
    sd_issue_command_int(dev, sd_commands[SEND_STATUS], dev->card_rca << 16,
                         500000);
    if (FAIL(dev)) {
      log_error("unable to get card status");
    } else {
      log_debug("card status: %08x", dev->last_r0);
    }
  } else {
    log_warn("no card currently selected");
  }
}


/* @brief   Handle interrupts of SDIO interface
 *
 */
static void sd_handle_interrupts(struct emmc_block_dev *dev) {
  uint32_t irpts = mmio_read(emmc_base + EMMC_INTERRUPT);
  uint32_t reset_mask = 0;

  if (irpts & SD_COMMAND_COMPLETE) {
    log_info("spurious command complete interrupt");
    reset_mask |= SD_COMMAND_COMPLETE;
  }

  if (irpts & SD_TRANSFER_COMPLETE) {
    log_info("spurious transfer complete interrupt");
    reset_mask |= SD_TRANSFER_COMPLETE;
  }

  if (irpts & SD_BLOCK_GAP_EVENT) {
    log_info("spurious block gap event interrupt");
    reset_mask |= SD_BLOCK_GAP_EVENT;
  }

  if (irpts & SD_DMA_INTERRUPT) {
    log_info("spurious DMA interrupt");
    reset_mask |= SD_DMA_INTERRUPT;
  }

  if (irpts & SD_BUFFER_WRITE_READY) {
    log_info("spurious buffer write ready interrupt");
    reset_mask |= SD_BUFFER_WRITE_READY;
    sd_reset_dat();
  }

  if (irpts & SD_BUFFER_READ_READY) {
    log_info("spurious buffer read ready interrupt");
    reset_mask |= SD_BUFFER_READ_READY;
    sd_reset_dat();
  }

  if (irpts & SD_CARD_INSERTION) {
    log_info("card insertion detected");
    reset_mask |= SD_CARD_INSERTION;
  }

  if (irpts & SD_CARD_REMOVAL) {
    log_info("card removal detected");
    reset_mask |= SD_CARD_REMOVAL;
    dev->card_removal = 1;
  }

  if (irpts & SD_CARD_INTERRUPT) {
    log_info("card interrupt detected");
    sd_handle_card_interrupt(dev);
    reset_mask |= SD_CARD_INTERRUPT;
  }

  if (irpts & 0x8000) {
    log_warn("spurious error interrupt: %08x", irpts);
    reset_mask |= 0xffff0000;
  }

  mmio_write(emmc_base + EMMC_INTERRUPT, reset_mask);
}


/* @brief   Send SDIO command
 *
 */
static void sd_issue_command(struct emmc_block_dev *dev, uint32_t command,
                             uint32_t argument, useconds_t timeout) {
  // First, handle any pending interrupts
  sd_handle_interrupts(dev);

  // Stop the command issue if it was the card remove interrupt that was
  //  handled
  if (dev->card_removal) {
    dev->last_cmd_success = 0;
    return;
  }

  // Now run the appropriate commands by calling sd_issue_command_int()
  if (command & IS_APP_CMD) {
    command &= 0xff;

    if (sd_acommands[command] == SD_CMD_RESERVED(0)) {
      log_info("invalid command ACMD%i", command);
      dev->last_cmd_success = 0;
      return;
    }
    dev->last_cmd = APP_CMD;

    uint32_t rca = 0;
    if (dev->card_rca) {
      rca = dev->card_rca << 16;
		}
		
    sd_issue_command_int(dev, sd_commands[APP_CMD], rca, timeout);
    if (dev->last_cmd_success) {
      dev->last_cmd = command | IS_APP_CMD;
      sd_issue_command_int(dev, sd_acommands[command], argument, timeout);
    }
  } else {
    if (sd_commands[command] == SD_CMD_RESERVED(0)) {
      log_error("invalid command CMD%i", command);
      dev->last_cmd_success = 0;
      return;
    }

    dev->last_cmd = command;    
    sd_issue_command_int(dev, sd_commands[command], argument, timeout);
  }

#ifdef EMMC_DEBUG
  if (FAIL(dev)) {
    log_error("error issuing command: interrupts %08x: ", dev->last_interrupt);
    if (dev->last_error == 0) {
      log_error("TIMEOUT");
    } else {
      for (int i = 0; i < SD_ERR_RSVD; i++) {
        if (dev->last_error & (1 << (i + 16))) {
          log_error("error: %s", err_irpts[i]);
        }
      }
    }
  }
#endif
}


/* @brief   Initialize the SDIO interface
 *
 */
int sd_card_init(struct block_device **dev) {

	log_info("**** sd_card_init ****");

  // Check the sanity of the sd_commands and sd_acommands structures
  if (sizeof(sd_commands) != (64 * sizeof(uint32_t))) {
    log_info("fatal error, sd_commands of incorrect size: %i"
         " expected %i",
         sizeof(sd_commands), 64 * sizeof(uint32_t));
    return -1;
  }
  if (sizeof(sd_acommands) != (64 * sizeof(uint32_t))) {
    log_info("fatal error, sd_acommands of incorrect size: %i"
         " expected %i",
         sizeof(sd_acommands), 64 * sizeof(uint32_t));
    return -1;
  }

#if SDHCI_IMPLEMENTATION == SDHCI_IMPLEMENTATION_BCM_2708
// Power cycle the card to ensure its in its startup state

  if(bcm_2708_power_cycle() != 0) {
    log_info("BCM2708 controller did not power cycle successfully");
    return -1;
  }

  log_info("BCM2708 controller power-cycled");
#endif

	log_info("* read controller version");

  // Read the controller version
  uint32_t ver = mmio_read(emmc_base + EMMC_SLOTISR_VER);
  uint32_t vendor = ver >> 24;
  uint32_t sdversion = (ver >> 16) & 0xff;
  uint32_t slot_status = ver & 0xff;
  
  log_info("vendor %x, sdversion %x, slot_status %x", vendor, sdversion, slot_status);
  
  hci_ver = sdversion;

  if (hci_ver < 2) {
    // log_info("only SDHCI versions >= 3.0 are supported");
    return -1;
  }

  // Reset the controller
  log_info("resetting controller");
  
  uint32_t control1 = mmio_read(emmc_base + EMMC_CONTROL1);
  control1 |= (1 << 24);
  // Disable clock
  control1 &= ~(1 << 2);
  control1 &= ~(1 << 0);
  mmio_write(emmc_base + EMMC_CONTROL1, control1);
  TIMEOUT_WAIT((mmio_read(emmc_base + EMMC_CONTROL1) & (0x7 << 24)) == 0,
               1000000);
  if ((mmio_read(emmc_base + EMMC_CONTROL1) & (0x7 << 24)) != 0) {
    log_info("controller did not reset properly");
    return -1;
  }

  log_info("control0: %08x, control1: %08x, control2: %08x",
       mmio_read(emmc_base + EMMC_CONTROL0),
       mmio_read(emmc_base + EMMC_CONTROL1),
       mmio_read(emmc_base + EMMC_CONTROL2));

  // Read the capabilities registers
  capabilities_0 = mmio_read(emmc_base + EMMC_CAPABILITIES_0);
  capabilities_1 = mmio_read(emmc_base + EMMC_CAPABILITIES_1);

  log_info("capabilities: %08x%08x", capabilities_1, capabilities_0);

	log_info("Setting SD bus to 3.3V");

	// Enable SD Bus Power VDD1 at 3.3V
  uint32_t control0 = mmio_read(emmc_base + EMMC_CONTROL0);
  control0 |= 0x0F << 8;
  mmio_write(emmc_base + EMMC_CONTROL0, control0);
  delay_microsecs(5000);


// Check for a valid card
  log_info("checking for an inserted card");

  TIMEOUT_WAIT(mmio_read(emmc_base + EMMC_STATUS) & (1 << 16), 500000);
  uint32_t status_reg = mmio_read(emmc_base + EMMC_STATUS);
  if ((status_reg & (1 << 16)) == 0) {
    log_info("no card inserted");
    return -1;
  }

  log_info("status: %08x", status_reg);

  // Clear control2
  mmio_write(emmc_base + EMMC_CONTROL2, 0);

  // Get the base clock rate
  uint32_t base_clock = sd_get_base_clock_hz();
  if (base_clock == 0) {
    log_info("assuming clock rate to be 100MHz");
    base_clock = 100000000;
  }

  // Set clock rate to something slow
  control1 = mmio_read(emmc_base + EMMC_CONTROL1);
  control1 |= 1; // enable clock

  // Set to identification frequency (400 kHz)
  uint32_t f_id = sd_get_clock_divider(base_clock, SD_CLOCK_ID);
  if (f_id == SD_GET_CLOCK_DIVIDER_FAIL) {
    log_info("unable to get a valid clock divider for ID frequency");
    return -1;
  }

	control1 &= ~(0x3FF << 6);
	control1 |= f_id;

	// was not masked out and or'd with (7 << 16) in original driver
	control1 &= ~(0xF << 16);
	control1 |= (11 << 16);		// data timeout = TMCLK * 2^24

  mmio_write(emmc_base + EMMC_CONTROL1, control1);

  TIMEOUT_WAIT(mmio_read(emmc_base + EMMC_CONTROL1) & 0x2, 1000000);

  if ((mmio_read(emmc_base + EMMC_CONTROL1) & 0x2) == 0) {
    log_info("controller's clock did not stabilise within 1 second");
    return -1;
  }

  log_info("control0: %08x, control1: %08x",
       mmio_read(emmc_base + EMMC_CONTROL0),
       mmio_read(emmc_base + EMMC_CONTROL1));

  // Enable the SD clock
  log_info("enabling SD clock");

  delay_microsecs(2000);
  control1 = mmio_read(emmc_base + EMMC_CONTROL1);
  control1 |= 4;
  mmio_write(emmc_base + EMMC_CONTROL1, control1);
  delay_microsecs(2000);

  log_info("reset interrupts");

  // Mask off sending interrupts to the ARM
//  mmio_write(emmc_base + EMMC_IRPT_EN, 0);
  // Reset interrupts
  mmio_write(emmc_base + EMMC_INTERRUPT, 0xffffffff);
  // Have all interrupts sent to the INTERRUPT register
  uint32_t irpt_mask = 0xffffffff & (~SD_CARD_INTERRUPT);

#ifdef SD_CARD_INTERRUPTS
  irpt_mask |= SD_CARD_INTERRUPT;    // FIXME
#endif

  mmio_write(emmc_base + EMMC_IRPT_MASK, irpt_mask);

  delay_microsecs(2000);

  // Prepare the device structure
  struct emmc_block_dev *ret;
  if (*dev == NULL)
    ret = (struct emmc_block_dev *)malloc(sizeof(struct emmc_block_dev));
  else
    ret = (struct emmc_block_dev *)*dev;

  memset(ret, 0, sizeof(struct emmc_block_dev));
  ret->bd.driver_name = driver_name;
  ret->bd.device_name = device_name;
  ret->bd.block_size = 512;
  ret->bd.read = sd_read;
#ifdef SD_WRITE_SUPPORT
  ret->bd.write = sd_write;
#endif
  ret->bd.supports_multiple_block_read = 1;
  ret->bd.supports_multiple_block_write = 1;
  ret->base_clock = base_clock;

  log_info("issue go idle command");

  // Send CMD0 to the card (reset to idle state)
//  sd_issue_command(ret, GO_IDLE_STATE, 0, 500000);

	sd_issue_command(ret, GO_IDLE_STATE, 0, 1500000);

	if (FAIL(ret)) {
	  log_info("no CMD0 response");
	  return -1;
	}

	// FIXME: Added sleep of 1 second before CMD8
	// sleep(1);
			
// Send CMD8 to the card
// Voltage supplied = 0x1 = 2.7-3.6V (standard)
// Check pattern = 10101010b (as per PLSS 4.3.13) = 0xAA
#ifdef EMMC_DEBUG
  log_info("note a timeout error on the following command (CMD8) is normal "
       "and expected if the SD card version is less than 2.0");
#endif

  sd_issue_command(ret, SEND_IF_COND, 0x1aa, 500000);
  int v2_later = 0;
  if (TIMEOUT(ret))
    v2_later = 0;
  else if (CMD_TIMEOUT(ret)) {
    if (sd_reset_cmd() == -1)
      return -1;
    mmio_write(emmc_base + EMMC_INTERRUPT, SD_ERR_MASK_CMD_TIMEOUT);
    v2_later = 0;
  } else if (FAIL(ret)) {
    log_info("failure sending CMD8 (%08x)", ret->last_interrupt);
    return -1;
  } else {
    if ((ret->last_r0 & 0xfff) != 0x1aa) {
      log_info("unusable card");
      log_info("CMD8 response %08x", ret->last_r0);
      return -1;
    } else
      v2_later = 1;
  }

// Here we are supposed to check the response to CMD5 (HCSS 3.6)
// It only returns if the card is a SDIO card
#ifdef EMMC_DEBUG
  log_info("note that a timeout error on the following command (CMD5) is "
       "normal and expected if the card is not a SDIO card.");
#endif
  sd_issue_command(ret, IO_SET_OP_COND, 0, 10000);
  if (!TIMEOUT(ret)) {
    if (CMD_TIMEOUT(ret)) {
      if (sd_reset_cmd() == -1)
        return -1;
      mmio_write(emmc_base + EMMC_INTERRUPT, SD_ERR_MASK_CMD_TIMEOUT);
    } else {
      log_info("SDIO card detected - not currently supported");
      log_info("CMD5 returned %08x", ret->last_r0);
      return -1;
    }
  }

  // Call an inquiry ACMD41 (voltage window = 0) to get the OCR
  log_info("sending inquiry ACMD41");

  sd_issue_command(ret, ACMD(41), 0, 500000);
  if (FAIL(ret)) {
    //        log_info("inquiry ACMD41 failed");
    return -1;
  }

  log_info("inquiry ACMD41 returned %08x", ret->last_r0);

  // Call initialization ACMD41
  int card_is_busy = 1;
  while (card_is_busy) {
    uint32_t v2_flags = 0;
    if (v2_later) {
      // Set SDHC support
      v2_flags |= (1 << 30);

// Set 1.8v support
#ifdef SD_1_8V_SUPPORT
      if (!ret->failed_voltage_switch)
        v2_flags |= (1 << 24);
#endif

// Enable SDXC maximum performance
#ifdef SDXC_MAXIMUM_PERFORMANCE
      v2_flags |= (1 << 28);
#endif
    }

    sd_issue_command(ret, ACMD(41), 0x00ff8000 | v2_flags, 500000);
    if (FAIL(ret)) {
      log_info("error issuing ACMD41");
      return -1;
    }

    if ((ret->last_r0 >> 31) & 0x1) {
      // Initialization is complete
      ret->card_ocr = (ret->last_r0 >> 8) & 0xffff;
      ret->card_supports_sdhc = (ret->last_r0 >> 30) & 0x1;

#ifdef SD_1_8V_SUPPORT
      if (!ret->failed_voltage_switch)
        ret->card_supports_18v = (ret->last_r0 >> 24) & 0x1;
#endif

      card_is_busy = 0;
    } else {
      // Card is still busy
      log_info("card is busy, retrying");
      delay_microsecs(500000);
    }
  }

#ifdef EMMC_DEBUG
  log_info("card identified: OCR: %04x, 1.8v support: %i, SDHC support: %i",
       ret->card_ocr, ret->card_supports_18v, ret->card_supports_sdhc);
#endif

  // At this point, we know the card is definitely an SD card, so will
  // definitely support SDR12 mode which runs at 25 MHz
  sd_switch_clock_rate(base_clock, SD_CLOCK_NORMAL);

  // A small wait before the voltage switch
  delay_microsecs(20000);

#if 0  
  // Switch to 1.8V mode if possible
  if (ret->card_supports_18v) {
    log_info("switching to 1.8V mode");
    // As per HCSS 3.6.1

    // Send VOLTAGE_SWITCH
    sd_issue_command(ret, VOLTAGE_SWITCH, 0, 500000);
    if (FAIL(ret)) {
      log_warn("error issuing VOLTAGE_SWITCH");

      ret->failed_voltage_switch = 1;
      sd_power_off();
      
      log_info("calling sd_card_init again");
      return sd_card_init((struct block_device **)&ret);
    }

    // Disable SD clock
    control1 = mmio_read(emmc_base + EMMC_CONTROL1);
    control1 &= ~(1 << 2);
    mmio_write(emmc_base + EMMC_CONTROL1, control1);

    // Check DAT[3:0]
    status_reg = mmio_read(emmc_base + EMMC_STATUS);
    uint32_t dat30 = (status_reg >> 20) & 0xf;
    if (dat30 != 0) {
      log_info("DAT[3:0] did not settle to 0");

      ret->failed_voltage_switch = 1;
      sd_power_off();

      log_info("calling sd_card_init again");
      return sd_card_init((struct block_device **)&ret);
    }

#if 0
    // Set 1.8V signal enable to 1
    uint32_t control0 = mmio_read(emmc_base + EMMC_CONTROL0);
    control0 |= (1 << 8);
    mmio_write(emmc_base + EMMC_CONTROL0, control0);

    // Wait 5 ms
    // delay_microsecs(5000);

    // Check the 1.8V signal enable is set
    control0 = mmio_read(emmc_base + EMMC_CONTROL0);
    if (((control0 >> 8) & 0x1) == 0) {
      log_info("controller did not keep 1.8V signal enable high");
      ret->failed_voltage_switch = 1;
      sd_power_off();
      
      log_info("calling sd_card_init again");
      return sd_card_init((struct block_device **)&ret);
    }
#else

		// Enable SD Bus Power VDD1 at 3.3V
    uint32_t control0 = mmio_read(emmc_base + EMMC_CONTROL0);
    control0 |= 0x0F << 8;
    mmio_write(emmc_base + EMMC_CONTROL0, control0);
    delay_microsecs(5000);


#endif


    // Re-enable the SD clock
    control1 = mmio_read(emmc_base + EMMC_CONTROL1);
    control1 |= (1 << 2);
    mmio_write(emmc_base + EMMC_CONTROL1, control1);

    // Wait 1 ms
    // delay_microsecs(10000);

    // Check DAT[3:0]
    status_reg = mmio_read(emmc_base + EMMC_STATUS);
    dat30 = (status_reg >> 20) & 0xf;
    if (dat30 != 0xf) {
      log_info("DAT[3:0] did not settle to 1111b (%01x)", dat30);
      ret->failed_voltage_switch = 1;
      sd_power_off();

      log_info("calling sd_card_init again");
      return sd_card_init((struct block_device **)&ret);
    }

    log_info("voltage switch complete");
  }
#endif

  // Send CMD2 to get the cards CID
  sd_issue_command(ret, ALL_SEND_CID, 0, 500000);
  if (FAIL(ret)) {
    log_info("error sending ALL_SEND_CID");
    return -1;
  }
  uint32_t card_cid_0 = ret->last_r0;
  uint32_t card_cid_1 = ret->last_r1;
  uint32_t card_cid_2 = ret->last_r2;
  uint32_t card_cid_3 = ret->last_r3;

  log_info("card CID: %08x%08x%08x%08x", card_cid_3, card_cid_2, card_cid_1, card_cid_0);

  uint32_t *dev_id = (uint32_t *)malloc(4 * sizeof(uint32_t));
  dev_id[0] = card_cid_0;
  dev_id[1] = card_cid_1;
  dev_id[2] = card_cid_2;
  dev_id[3] = card_cid_3;
  ret->bd.device_id = (uint8_t *)dev_id;
  ret->bd.dev_id_len = 4 * sizeof(uint32_t);

// TODO:  Can we send CSD command to get device capacity/attributes
// How to determine HC capacity type cards?
// not sure what last params are for?
/*
  sd_issue_command(ret, SEND_CSD, 0, 500000);
  if (FAIL(ret)) {
    log_info("error sending SEND_CSD");
    return -1;
  }
  uint32_t card_csd_0 = ret->last_r0;
  uint32_t card_csd_1 = ret->last_r1;
  uint32_t card_csd_2 = ret->last_r2;
  uint32_t card_csd_3 = ret->last_r3;


  uint32_t *dev_csd = (uint32_t *)malloc(4 * sizeof(uint32_t));
  dev_csd[0] = card_csd_0;
  dev_csd[1] = card_csd_1;
  dev_csd[2] = card_csd_2;
  dev_csd[3] = card_csd_3;
//  ret->bd.device_id = (uint8_t *)dev_id;
//  ret->bd.dev_id_len = 4 * sizeof(uint32_t);
*/

  // Send CMD3 to enter the data state
  sd_issue_command(ret, SEND_RELATIVE_ADDR, 0, 500000);
  if (FAIL(ret)) {
    log_error("error sending SEND_RELATIVE_ADDR");
    free(ret);
    return -1;
  }

  uint32_t cmd3_resp = ret->last_r0;

  log_info("CMD3 response: %08x", cmd3_resp);

  ret->card_rca = (cmd3_resp >> 16) & 0xffff;
  uint32_t crc_error = (cmd3_resp >> 15) & 0x1;
  uint32_t illegal_cmd = (cmd3_resp >> 14) & 0x1;
  uint32_t error = (cmd3_resp >> 13) & 0x1;
  uint32_t status = (cmd3_resp >> 9) & 0xf;
  uint32_t ready = (cmd3_resp >> 8) & 0x1;

  if (crc_error) {
    log_error("CRC error");
    free(ret);
    free(dev_id);
    return -1;
  }

  if (illegal_cmd) {
    log_error("illegal command");
    free(ret);
    free(dev_id);
    return -1;
  }

  if (error) {
    log_error("generic error");
    free(ret);
    free(dev_id);
    return -1;
  }

  if (!ready) {
    log_error("not ready for data");
    free(ret);
    free(dev_id);
    return -1;
  }

  log_info("RCA: %04x", ret->card_rca);

  // Now select the card (toggles it to transfer state)
  sd_issue_command(ret, SELECT_CARD, ret->card_rca << 16, 500000);
  if (FAIL(ret)) {
    log_error("error sending CMD7");
    free(ret);
    return -1;
  }

  uint32_t cmd7_resp = ret->last_r0;
  status = (cmd7_resp >> 9) & 0xf;

  if ((status != 3) && (status != 4)) {
    log_error("invalid status (%i)", status);
    free(ret);
    free(dev_id);
    return -1;
  }

  // If not an SDHC card, ensure BLOCKLEN is 512 bytes
  if (!ret->card_supports_sdhc) {
    sd_issue_command(ret, SET_BLOCKLEN, 512, 500000);
    if (FAIL(ret)) {
      log_error("error sending SET_BLOCKLEN");
      free(ret);
      return -1;
    }
  }
  ret->block_size = 512;
  uint32_t controller_block_size = mmio_read(emmc_base + EMMC_BLKSIZECNT);
  controller_block_size &= (~0xfff);
  controller_block_size |= 0x200;
  mmio_write(emmc_base + EMMC_BLKSIZECNT, controller_block_size);

  // Get the cards SCR register
  ret->scr = (struct sd_scr *)malloc(sizeof(struct sd_scr));
  ret->buf = &ret->scr->scr[0];
  ret->block_size = 8;
  ret->blocks_to_transfer = 1;
  sd_issue_command(ret, SEND_SCR, 0, 500000);
  ret->block_size = 512;
  if (FAIL(ret)) {
    log_error("error sending SEND_SCR");
    free(ret->scr);
    free(ret);
    return -1;
  }

  // Determine card version
  // Note that the SCR is big-endian
  uint32_t scr0 = byte_swap(ret->scr->scr[0]);
  ret->scr->sd_version = SD_VER_UNKNOWN;
  uint32_t sd_spec = (scr0 >> (56 - 32)) & 0xf;
  uint32_t sd_spec3 = (scr0 >> (47 - 32)) & 0x1;
  uint32_t sd_spec4 = (scr0 >> (42 - 32)) & 0x1;
  ret->scr->sd_bus_widths = (scr0 >> (48 - 32)) & 0xf;
  if (sd_spec == 0)
    ret->scr->sd_version = SD_VER_1;
  else if (sd_spec == 1)
    ret->scr->sd_version = SD_VER_1_1;
  else if (sd_spec == 2) {
    if (sd_spec3 == 0)
      ret->scr->sd_version = SD_VER_2;
    else if (sd_spec3 == 1) {
      if (sd_spec4 == 0)
        ret->scr->sd_version = SD_VER_3;
      else if (sd_spec4 == 1)
        ret->scr->sd_version = SD_VER_4;
    }
  }

  log_info("&scr: %08x", &ret->scr->scr[0]);
  log_info("SCR[0]: %08x, SCR[1]: %08x", ret->scr->scr[0], ret->scr->scr[1]);
  log_info("SCR: %08x%08x", byte_swap(ret->scr->scr[0]), byte_swap(ret->scr->scr[1]));
  log_info("SCR: version %s, bus_widths %01x", sd_versions[ret->scr->sd_version], ret->scr->sd_bus_widths);


#ifdef SD_4BIT_DATA
  if (ret->scr->sd_bus_widths & 0x4) {
    // Set 4-bit transfer mode (ACMD6)
    // See HCSS 3.4 for the algorithm
    log_info("switching to 4-bit data mode");

    // Disable card interrupt in host
    uint32_t old_irpt_mask = mmio_read(emmc_base + EMMC_IRPT_MASK);
    uint32_t new_iprt_mask = old_irpt_mask & ~(1 << 8);
    mmio_write(emmc_base + EMMC_IRPT_MASK, new_iprt_mask);

    // Send ACMD6 to change the card's bit mode
    sd_issue_command(ret, SET_BUS_WIDTH, 0x2, 500000);
    if (FAIL(ret)) {
      log_info("switch to 4-bit data mode failed");
    } else {
      // Change bit mode for Host
      uint32_t control0 = mmio_read(emmc_base + EMMC_CONTROL0);
      control0 |= 0x2;
      mmio_write(emmc_base + EMMC_CONTROL0, control0);

      // Re-enable card interrupt in host
      mmio_write(emmc_base + EMMC_IRPT_MASK, old_irpt_mask);

      log_info("switch to 4-bit complete");
    }
  }
#endif

  log_info("found a valid version %s SD card", sd_versions[ret->scr->sd_version]);
  log_info("setup successful (status %i)", status);

  // Reset interrupt register
  mmio_write(emmc_base + EMMC_INTERRUPT, 0xffffffff);

  *dev = (struct block_device *)ret;

  return 0;
}


/*
 *
 */
static int sd_ensure_data_mode(struct emmc_block_dev *edev) {
  if (edev->card_rca == 0) {
    // Try again to initialise the card
    int ret = sd_card_init((struct block_device **)&edev);
    if (ret != 0)
      return ret;
  }

  sd_issue_command(edev, SEND_STATUS, edev->card_rca << 16, 500000);
  if (FAIL(edev)) {
    log_info("ensure_data_mode() error sending CMD13");
    edev->card_rca = 0;
    return -1;
  }

  uint32_t status = edev->last_r0;
  uint32_t cur_state = (status >> 9) & 0xf;

  if (cur_state == 3) {
    // Currently in the stand-by state - select it
    sd_issue_command(edev, SELECT_CARD, edev->card_rca << 16, 500000);
    if (FAIL(edev)) {
      log_info("ensure_data_mode() no response from CMD17");
      edev->card_rca = 0;
      return -1;
    }
  } else if (cur_state == 5) {
    // In the data transfer state - cancel the transmission
    sd_issue_command(edev, STOP_TRANSMISSION, 0, 500000);
    if (FAIL(edev)) {
      log_info("ensure_data_mode() no response from CMD12");
      edev->card_rca = 0;
      return -1;
    }

    // Reset the data circuit
    sd_reset_dat();
  } else if (cur_state != 4) {
    // Not in the transfer state - re-initialise
    int ret = sd_card_init((struct block_device **)&edev);
    if (ret != 0)
      return ret;
  }

  // Check again that we're now in the correct mode
  if (cur_state != 4) {
    sd_issue_command(edev, SEND_STATUS, edev->card_rca << 16, 500000);
    if (FAIL(edev)) {
      log_info("ensure_data_mode() no response from CMD13");
      edev->card_rca = 0;
      return -1;
    }
    status = edev->last_r0;
    cur_state = (status >> 9) & 0xf;

    if (cur_state != 4) {
      log_info("unable to initialise SD card to data mode (state %i)", cur_state);
      edev->card_rca = 0;
      return -1;
    }
  }

  return 0;
}

#ifdef SDMA_SUPPORT
// We only support DMA transfers to buffers aligned on a 4 kiB boundary
static int sd_suitable_for_dma(void *buf) {
  if ((uintptr_t)buf & 0xfff)
    return 0;
  else
    return 1;
}
#endif

static int sd_do_data_command(struct emmc_block_dev *edev, int is_write,
                              uint8_t *buf, size_t buf_size,
                              uint32_t block_no) {
  // PLSS table 4.20 - SDSC cards use byte addresses rather than block addresses
  if (!edev->card_supports_sdhc)
    block_no *= 512;

  // This is as per HCSS 3.7.2.1
  if (buf_size < edev->block_size) {
    log_info("do_data_command() called with buffer size (%i) less than "
         "block size (%i)",
         buf_size, edev->block_size);
    return -1;
  }

  edev->blocks_to_transfer = buf_size / edev->block_size;
  if (buf_size % edev->block_size) {
    log_info("do_data_command() called with buffer size (%i) not an "
         "exact multiple of block size (%i)",
         buf_size, edev->block_size);
    return -1;
  }
  edev->buf = buf;

  // Decide on the command to use
  int command;
  if (is_write) {
    if (edev->blocks_to_transfer > 1)
      command = WRITE_MULTIPLE_BLOCK;
    else
      command = WRITE_BLOCK;
  } else {
    if (edev->blocks_to_transfer > 1)
      command = READ_MULTIPLE_BLOCK;
    else
      command = READ_SINGLE_BLOCK;
  }

  int retry_count = 0;
  int max_retries = 3;
  while (retry_count < max_retries) {
#ifdef SDMA_SUPPORT
    // use SDMA for the first try only
    if ((retry_count == 0) && sd_suitable_for_dma(buf))
      edev->use_sdma = 1;
    else {
      log_info("retrying without SDMA");
      edev->use_sdma = 0;
    }
#else
    edev->use_sdma = 0;
#endif

    sd_issue_command(edev, command, block_no, 5000000);

    if (SUCCESS(edev))
      break;
    else {
      log_info("error sending CMD%i, ", command);
      log_info("error = %08x.  ", edev->last_error);
      retry_count++;
      if (retry_count < max_retries)
        log_info("Retrying...");
      else
        log_info("Giving up.");
    }
  }
  if (retry_count == max_retries) {
    edev->card_rca = 0;
    return -1;
  }

  return 0;
}


/* @brief   Read from an SD card
 *
 */
int sd_read(struct block_device *dev, uint8_t *buf, size_t buf_size, uint32_t block_no)
{
  // Check the status of the card
  struct emmc_block_dev *edev = (struct emmc_block_dev *)dev;
  if (sd_ensure_data_mode(edev) != 0)
    return -1;

  log_debug("read() card ready, reading from block %d", block_no);

  if (sd_do_data_command(edev, 0, buf, buf_size, block_no) < 0)
  {
    log_error("failed to read block %d ***", (uint32_t)block_no);
    return -1;
  }
  
  return buf_size;
}

/* @brief   Write to an SD card
 *
 */
#ifdef SD_WRITE_SUPPORT
int sd_write(struct block_device *dev, uint8_t *buf, size_t buf_size, uint32_t block_no)
{
  // Check the status of the card
  struct emmc_block_dev *edev = (struct emmc_block_dev *)dev;
  if (sd_ensure_data_mode(edev) != 0)
    return -1;

  log_debug("sd_write() block %d, buf:%08x, sz:%d", (uint32_t)block_no, (uint32_t)buf, buf_size);

  if (sd_do_data_command(edev, 1, buf, buf_size, block_no) < 0)
  {
  	log_error("failed to write block %d ***", (uint32_t)block_no);
    return -1;
	}
	
  return buf_size;
}
#endif


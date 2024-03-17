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

#ifndef SYS_RPI_MAILBOX_H
#define SYS_RPI_MAILBOX_H

#include <stdint.h>
#include <machine/cheviot_hal.h>


// Commands
#define MBOX_TAG_GET_POWER_STATE		0x00020001
#define MBOX_TAG_SET_POWER_STATE		0x00028001
#define MBOX_TAG_GET_CLOCK_STATE 		0x00030001
#define MBOX_TAG_SET_CLOCK_STATE 		0x00038001
#define MBOX_TAG_GET_CLOCK_RATE 		0x00030002


// Device IDs
#define MBOX_DEVICE_ID_SDCARD				0x00000000
#define MBOX_DEVICE_ID_UART0				0x00000001
#define MBOX_DEVICE_ID_UART1				0x00000002
#define MBOX_DEVICE_ID_USB_HCD			0x00000003
#define MBOX_DEVICE_ID_I2C0					0x00000004
#define MBOX_DEVICE_ID_I2C1					0x00000005
#define MBOX_DEVICE_ID_I2C2					0x00000006
#define MBOX_DEVICE_ID_SPI					0x00000007
#define MBOX_DEVICE_ID_CCP2TX				0x00000008

// Clock IDs
#define MBOX_CLOCK_ID_RESERVED			0x00000000
#define MBOX_CLOCK_ID_EMMC					0x00000001
#define MBOX_CLOCK_ID_UART					0x00000002
#define MBOX_CLOCK_ID_ARM						0x00000003
#define MBOX_CLOCK_ID_CORE					0x00000004
#define MBOX_CLOCK_ID_V3D						0x00000005
#define MBOX_CLOCK_ID_H264					0x00000006
#define MBOX_CLOCK_ID_ISP						0x00000007
#define MBOX_CLOCK_ID_SDRAM					0x00000008
#define MBOX_CLOCK_ID_PIXEL					0x00000009
#define MBOX_CLOCK_ID_PWM						0x0000000A
#define MBOX_CLOCK_ID_HEVC					0x0000000B
#define MBOX_CLOCK_ID_EMMC2					0x0000000C
#define MBOX_CLOCK_ID_M2MC					0x0000000D
#define MBOX_CLOCK_ID_PIXEL_BVB			0x0000000E

// Power state flags
#define MBOX_POWER_STATE_ON					(1<<0)
#define MBOX_POWER_STATE_NEXIST			(1<<1)

// Clock state flags
#define MBOX_CLOCK_STATE_ON					(1<<0)
#define MBOX_CLOCK_STATE_NEXIST			(1<<1)


/*
 * Prototypes
 */

int rpi_mailbox(uint32_t cmd, uint32_t *request, size_t req_sz,
								uint32_t *response, size_t resp_sz);

int rpi_mailbox_get_power_state(uint32_t device_id, uint32_t *state);
int rpi_mailbox_set_power_state(uint32_t device_id, uint32_t state);

int rpi_mailbox_get_clock_state(uint32_t device_id, uint32_t *state);								
int rpi_mailbox_set_clock_state(uint32_t device_id, uint32_t state);

int rpi_mailbox_get_clock_rate(uint32_t device_id, uint32_t *rate);

								


#endif

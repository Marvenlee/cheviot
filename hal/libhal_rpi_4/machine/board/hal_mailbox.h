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

#ifndef MACHINE_BOARD_HAL_MAILBOX_H
#define MACHINE_BOARD_HAL_MAILBOX_H

#include <stdint.h>
#include <stdbool.h>


/* Mailbox register offsets from MBOX_BASE
 * Note: MBOX_BASE is not page aligned
 * Either set MBOX_BASE to 0x2000B000 and set these offsets to 0xB880 0xB890 etc
 * Or handle offsets when doing memory map setup.
 *
 * TODO: Replace with struct bcm2835_mbox_regs
 */
#define MBOX_READ     0x00    // 0x2000B880
#define MBOX_WRITE    0x20    // 0x2000B8A0
#define MBOX_PEEK     0x10    // 0x2000B890
#define MBOX_STATUS   0x18    // 0x2000B898
#define MBOX_SENDER   0x14    // 0x2000B894
#define MBOX_CONFIG   0x1C    // 0x2000B89C


// Mailbox channels
#define MBOX_FB       1
#define MBOX_PROP     8


// Mailbox status register flags
#define MBOX_SUCCESS  0x80000000
#define MBOX_FULL     0x80000000
#define MBOX_EMPTY    0x40000000

void hal_set_mbox_base(void *base_addr);
void *hal_get_mbox_base(void);
uint32_t hal_mbox_read(uint8_t channel);
void hal_mbox_write(uint8_t channel, uint32_t data);
uint32_t hal_mbox_status(void);
uint32_t *hal_mbox_find_tag(uint32_t tag, uint32_t *msg);


// Rename to MAILBOX_BUFFER_L2CC_PA
#define MB_ADDR_PA        0x40007000      /* Mailbox buffer address */
                                          /* Mailbox buffer will be at 0x00007000 physical
                                             address but will be in the L2 cache coherent
                                             (non-allocating) 0x4xxxxxxx alias.
                                             We need to ensure that 0x00007000 is reserved
                                             for the mailbox in bootloader and kernel */
                                          /* FIXME: See hal_videocore.c for SD-Card fix suggestion */

#endif



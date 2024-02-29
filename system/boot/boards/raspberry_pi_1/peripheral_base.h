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

#ifndef BOARDS_RASPBERRY_PI_1_PERIPH_BASE_H
#define BOARDS_RASPBERRY_PI_1_PERIPH_BASE_H

#include <stdint.h>
#include <stdbool.h>

/*
 * ARM Physical base addresses of peripherals and ARM control block
 */
#define PERIPHERAL_BASE       0x20000000                        /* Peripheral ARM phyiscal address */
#define MBOX_BASE             (PERIPHERAL_BASE + 0x0000B880)    /* VideoCore Mailbox base */
                                                                /* NOTE: This is not page-aligned */

#define GPIO_BASE             (PERIPHERAL_BASE + 0x00200000)    /* GPIO interface */
#define AUX_BASE              (PERIPHERAL_BASE + 0x00215000)    /* Aux mini-uart and SPI */


#endif


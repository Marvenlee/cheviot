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

#ifndef BOARDS_RASPBERRY_PI_1_GPIO_H
#define BOARDS_RASPBERRY_PI_1_GPIO_H

#include <stdint.h>
#include <stdbool.h>


/* BCM2835 GPIO Register block
 */
struct bcm2835_gpio_registers
{
  uint32_t fsel[6];     // 0x00
  uint32_t resvd1;      // 0x18
  uint32_t set[2];      // 0x1C
  uint32_t resvd2;      // 0x24
  uint32_t clr[2];      // 0x28
  uint32_t resvd3;      // 0x30
  uint32_t lev[2];      // 0x34
  uint32_t resvd4;      // 0x3C
  uint32_t eds[2];      // 0x40
  uint32_t resvd5;      // 0x48
  uint32_t ren[2];      // 0x4C
  uint32_t resvd6;      // 0x54
  uint32_t fen[2];      // 0x58
  uint32_t resvd7;      // 0x60
  uint32_t hen[2];      // 0x64
  uint32_t resvd8;      // 0x6C
  uint32_t len[2];      // 0x70
  uint32_t resvd9;      // 0x78
  uint32_t aren[2];     // 0x7C
  uint32_t resvd10;     // 0x84
  uint32_t afen[2];     // 0x88
  uint32_t resvd11;     // 0x90
  uint32_t pud;         // 0x94
  uint32_t pud_clk[2];  // 0x98
};


/* GPIO Alternate Function Select (pin mux)
 */
enum FSel
{
    INPUT   = 0,
    OUTPUT  = 1,
    FN5     = 2,
    FN4     = 3,
    FN0     = 4,
    FN1     = 5,
    FN2     = 6,
    FN3     = 7,
};


/* GPIO pin pull-up configuration
 */
enum PullUpDown
{
    PULL_NONE = 0,
    PULL_UP   = 1,
    PULL_DOWN = 2
};


// BCM2835 GPIO registers
extern struct bcm2835_gpio_registers *gpio_regs;

/*
 * Prototypes
 */
void init_gpio(void *addr);
void configure_gpio(uint32_t pin, enum FSel fn, enum PullUpDown action);
void set_gpio(uint32_t pin, bool state);
bool get_gpio(uint32_t pin);




#endif



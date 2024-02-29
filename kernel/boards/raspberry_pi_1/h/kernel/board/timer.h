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

#ifndef MACHINE_BOARD_RASPBERRY_PI_1_TIMER_H
#define MACHINE_BOARD_RASPBERRY_PI_1_TIMER_H

#include <stdint.h>


/*
 * System Timer
 */
struct bcm2835_timer_registers
{
  uint32_t cs;
  uint32_t clo;
  uint32_t chi;
  uint32_t c0;
  uint32_t c1;
  uint32_t c2;
  uint32_t c3;
};

// CS register bits
#define ST_CS_M3 (0x08)
#define ST_CS_M2 (0x04)
#define ST_CS_M1 (0x02)
#define ST_CS_M0 (0x01)


#endif


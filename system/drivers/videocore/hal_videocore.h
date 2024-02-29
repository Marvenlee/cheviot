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

#ifndef MACHINE_BOARD_RASPBERRY_PI_1_VIDEOCORE_H
#define MACHINE_BOARD_RASPBERRY_PI_1_VIDEOCORE_H

#include <stdint.h>


void hal_vc_get_screen_dimensions(uint32_t *screen_width, uint32_t *screen_height);
void hal_vc_set_screen_mode(uint32_t screen_width, uint32_t screen_height);
uint16_t hal_vc_calc_rgb16(uint8_t r, uint8_t g, uint8_t b);
void hal_vc_putpixel(int x, int y, uint16_t color);

#endif


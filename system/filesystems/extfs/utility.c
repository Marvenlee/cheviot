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

#define LOG_LEVEL_INFO

#include "ext2.h"
#include "globals.h"


/* @brief   Determine's if we are running on a big-endian CPU
 *
 */
void determine_cpu_endianness(void)
{
  unsigned short test_endian = 1;
  be_cpu = (*(unsigned char *) &test_endian == 0 ? true : false);
} 


/* @brief   Byte-swap a 16-bit unsigned integer
 *
 * @param   swap, true to perform byte swap, false for as-is
 * @param   w, 16-bit unsigned integer to be byte swapped
 */
uint16_t bswap2(bool swap, uint16_t w)
{
  if (!swap) {
    return w;
  }
  
  return( ((w & 0xFF) << 8) | ( (w>>8) & 0xFF));
}


/* @brief   Byte-swap a 32-bit unsigned integer
 *
 * @param   swap, true to perform byte swap, false for as-is
 * @param   x, 32-bit long to be byte swapped
 */
uint32_t bswap4(bool swap, uint32_t x)
{
  if (!swap) {
    return x;
  }
    
  return ((x & 0x000000FF) << 24) | ((x & 0x0000FF00) << 8)
          | ((x & 0x00FF0000) >> 8) | ((x & 0xFF000000) >> 24);  
}


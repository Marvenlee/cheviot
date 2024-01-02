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


/* @brief   Allocate and set a bit in a bitmap
 *
 * @param   bitmap, bitmap to allocate and set a bit within
 * @param   max_bits, maximum size of the bitmap to search
 * @param   word, word in bitmap to start search from
 * @return  return index of bit allocated or 0xFFFFFFFF on error
 */ 
uint32_t alloc_bit(uint32_t *bitmap, uint32_t max_bits, uint32_t start_word)
{
  for (uint32_t w = start_word; w < max_bits / 32; w++) {
	  if (bitmap[w] == 0xFFFFFFFFUL) {
		  continue;
    }
   
    for (uint32_t b = 0; b < 32 && w * 32 + b < max_bits; b++) {
      if ((bitmap[w] & (1<<b)) != 0) {
    	  bitmap[w] |= 1<<b;
    	  return w * 32 + b;
      }
    }
  }
  return NO_BLOCK;
}


/* @brief   Clear a bit in a bitmap
 *
 * @param   bitmap, the bitmap to clear a bit within
 * @param   index, the index of the bit to clear
 * @return  returns 0 on success, -1 if bit is already clear
 */
int clear_bit(uint32_t *bitmap, uint32_t index)
{
  uint32_t word;
  uint32_t mask;

  word = index / 32;
  mask = 1 << (index % 32);

  if (!(bitmap[word] & mask)) {
  	return -1;
  }
  
  bitmap[word] &= ~mask;
  return 0;
}



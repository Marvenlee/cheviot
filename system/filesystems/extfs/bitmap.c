/* This file manages bits in block bitmaps
 *
 * Created (CheviotOS Filesystem Handler based)
 *   December 2023 (Marven Gilhespie) 
 */

#define LOG_LEVEL_WARN

#include "ext2.h"
#include "globals.h"


/* @brief   Allocate and set a bit in a bitmap
 *
 * @param   bitmap, bitmap to allocate and set a bit within
 * @param   max_bits, maximum size of the bitmap to search
 * @param   word, word in bitmap to start search from
 * @return  return index of bit allocated or -1 on error
 */ 
int alloc_bit(uint32_t *bitmap, uint32_t max_bits, uint32_t start_word)
{
  for (uint32_t w = start_word; w * 32 < max_bits; w++) {
	  if (bitmap[w] == 0xFFFFFFFFUL) {
		  continue;
    }
   
    for (uint32_t b = 0; b < 32 && w * 32 + b < max_bits; b++) {
      if ((bitmap[w] & (1<<b)) == 0) {
    	  bitmap[w] |= 1<<b;
    	  return w * 32 + b;
      }
    }
  }
  
  return -1;
}


/* @brief   Clear a bit in a bitmap
 *
 * @param   bitmap, the bitmap to clear a bit within
 * @param   index, the index of the bit to clear
 * @return  returns 0 on success, -1 if bit is already clear
 */
int clear_bit(uint32_t *bitmap, int index)
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



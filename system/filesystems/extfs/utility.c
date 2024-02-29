/* This file contains utility functions for byte swapping.
 *
 * Created (CheviotOS Filesystem Handler based)
 *   December 2023 (Marven Gilhespie) 
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


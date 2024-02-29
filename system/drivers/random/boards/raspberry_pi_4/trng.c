/*
 * Copyright 2019  Marven Gilhespie
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

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscalls.h>
#include <unistd.h>
#include <sys/debug.h>
#include <sys/lists.h>
#include <machine/cheviot_hal.h>
#include "peripheral_base.h"
#include "trng.h"


/*
 * Memory mapped IO locations
 */
struct bcm2711_trng_registers *trng_regs;


/*
 *
 */ 
int trng_hw_init(void)
{
  trng_regs = virtualallocphys((void *)0x50000000, 4096,
                          PROT_READ | PROT_WRITE | CACHE_UNCACHEABLE,
                          (void *)(RNG_BASE));  
  
  if (trng_regs == NULL) {
    return -ENOMEM;
  }  

  /* set warm-up count & enable */
	hal_mmio_write(&trng_regs->status, RNG_WARMUP_COUNT);
	hal_mmio_write(&trng_regs->ctrl, RNG_RBGEN);   
  return 0;
}


/*
 *
 */
int trng_data_read(uint32_t *buffer)
{
	uint32_t words;
		
	/* wait for a random number to be in fifo */
  do {
		words = hal_mmio_read(&trng_regs->status)>>24;
	} while (words == 0);

	/* read the random number */
	*buffer = hal_mmio_read(&trng_regs->data);
	return 4;
}


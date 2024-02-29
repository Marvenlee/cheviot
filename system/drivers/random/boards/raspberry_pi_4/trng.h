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

#ifndef BOARDS_RASPBERRY_PI_4_TRNG_H
#define BOARDS_RASPBERRY_PI_4_TRNG_H


#include <stdint.h>
#include <stdbool.h>


/* @brief   TRNG registers of the bcm2711
 */
struct bcm2711_trng_registers
{
	uint32_t  ctrl;
	uint32_t  status;
	uint32_t  data;
	uint32_t  ff_threshold;
};

/* ctrl register
 */
#define RNG_RBGEN               0x1	  /* enable rng */
#define RNG_RBG2X               0x2		/* double speed, less random mode */

/* Discard the initial numbers generated */
#define RNG_WARMUP_COUNT      0x40000


/* Prototypes
 */
int trng_hw_init(void);
int trng_data_read(uint32_t *buffer);


#endif



/*
 * Copyright 2023  Marven Gilhespie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
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

#include <machine/cheviot_hal.h>
#include <kernel/arch.h>
#include <kernel/board/boot.h>
#include <kernel/board/globals.h>
#include <kernel/board/gpio.h>
#include <kernel/dbg.h>
#include <kernel/globals.h>
#include <kernel/types.h>
#include <kernel/utility.h>
#include <kernel/vm.h>


// Function for clock_gettime() monotonic RAW values.

arch_clock_gettime(clockid_t id, struct timespec *_ts)
{
	switch(id) {
		case CLOCK_REALTIME:					// time of day clock (adjusted relative to CLOCK_MONOTONIC
			break;
			
		case CLOCK_MONOTONIC:					// system timer since boot.
			break;
					
		case CLOCK_MONOTONIC_RAW:			// 64-bit high frequency counter
			break;
	}
}

















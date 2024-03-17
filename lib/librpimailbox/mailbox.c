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

#include <stdint.h>
#include <machine/cheviot_hal.h>
#include "sys/rpi_mailbox.h"
#include <errno.h>


extern int _swi_rpi_mailbox(uint32_t cmd, uint32_t *request, size_t req_sz, 
														uint32_t *response, size_t resp_sz);



/*
 *
 */
int rpi_mailbox(uint32_t cmd, uint32_t *request, size_t req_sz, uint32_t *response, size_t resp_sz)
{
	int sc;
	
	sc = _swi_rpi_mailbox(cmd, request, req_sz, response, resp_sz);
		
	if (sc != 0) {
		errno = -sc;
		return -1;
	}

	return 0;
}


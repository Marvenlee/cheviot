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


int rpi_mailbox_get_clock_state(uint32_t device_id, uint32_t *state)
{
	int sc;
	uint32_t request[1];
	uint32_t response[2];

	request[0] = device_id;
	
	sc = rpi_mailbox(MBOX_TAG_GET_CLOCK_STATE, &request, sizeof request,
									 response, sizeof response);
	
	if (sc != 0) {
		return -1;
	}
	
	*state = response[1];
	return 0;
}


int rpi_mailbox_set_clock_state(uint32_t device_id, uint32_t state)
{
	int sc;
	uint32_t request[2];
	uint32_t response[2];
	
	request[0] = device_id;
	request[1] = state;
	
	sc = rpi_mailbox(MBOX_TAG_SET_CLOCK_STATE, &request, sizeof request,
									 response, sizeof response);
	
	if (sc != 0) {
		return -1;
	}

	return 0;	
}


int rpi_mailbox_get_clock_rate(uint32_t device_id, uint32_t *rate)
{
	int sc;
	uint32_t request[1];
	uint32_t response[2];
	
	sc = rpi_mailbox(MBOX_TAG_GET_CLOCK_RATE, &request, sizeof request,
									 response, sizeof response);
	
	if (sc != 0) {
		return -1;
	}
		
	*rate = response[1];
	return 0;
}


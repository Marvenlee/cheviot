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

//#define KDEBUG

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


// TODO: set mailbuffer pointer and physical address

/* @brief		Syscall for specific raspberry Pi mailbox commands.
 *
 */
int sys_rpi_mailbox(uint32_t tag, void *request, int req_sz, void *response, int response_sz)
{
  uint32_t result;
	uint32_t clock_id;
	uint32_t freq;
	uint32_t actual_response_sz;
	
	memset(mailbuffer, 0, sizeof mailbuffer);
		
  mailbuffer[0] = 64 * 4;				// max_buffer size
  mailbuffer[1] = 0;						// request code (0)
  mailbuffer[2] = tag;   				// tag id / command
  mailbuffer[3] = req_sz;				// value buffer size in bytes (4 bytes)
  mailbuffer[4] = 0;            // request/response code (request)

  CopyIn(&mailbuffer[5], request, req_sz);
  
  mailbuffer[5 + req_sz/4] = 0;		// end tag

  do {
    hal_mbox_write(MBOX_PROP, mailbuffer_pa);
    result = hal_mbox_read(MBOX_PROP);
  } while (result == 0);

	// TODO: Add additional checks of response

	actual_response_sz = mailbuffer[3];	
	if (actual_response_sz > response_sz) {
		return -EIO;
	}
	
	CopyOut(response, &mailbuffer[5], actual_response_sz);
	return 0;
}


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

#include <machine/cheviot_hal.h>

static uint32_t mailbuffer[64] __attribute__((aligned(16)));

/* @brief   Determine amount of physical memory.
 */
size_t get_physical_mem_size(void)
{
#if 0
  uint32_t result;
  size_t size;

  mailbuffer[0] = 8 * 4;
  mailbuffer[1] = 0;
  mailbuffer[2] = 0x00010005;
  mailbuffer[3] = 8;
  mailbuffer[4] = 0;
  mailbuffer[5] = 0;
  mailbuffer[6] = 0;
  mailbuffer[7] = 0;

  do {
    hal_mbox_write(MBOX_PROP,(uint32_t)mailbuffer);
    result = hal_mbox_read(MBOX_PROP);
  } while (result == 0);

  // TODO: Assuming memory starts at 0x00000000
  //	mem_base = mailbuffer[5];
  size = mailbuffer[6];
#else
  size_t size;

  size = 0x08000000;    // Assume 256MB
#endif
  return size;
}


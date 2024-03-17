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
#include <sys/types.h>
#include <stddef.h>


// Mailbox registers
static uint8_t *mbox_base;



/*
 *
 */
void hal_set_mbox_base(void *base_addr)
{
  mbox_base = base_addr;
}


/*
 *
 */
uint32_t hal_mbox_read(uint8_t channel)
{
  while (1) {
    while (hal_mmio_read(mbox_base + MBOX_STATUS) & MBOX_EMPTY);
        
    uint32_t data = hal_mmio_read(mbox_base + MBOX_READ);
    uint8_t read_channel = (uint8_t)(data & 0xf);
    if (read_channel == channel) {
      return (data & 0xfffffff0);
    }
  }
}


/*
 *
 */
void hal_mbox_write(uint8_t channel, uint32_t data)
{
  while (hal_mmio_read(mbox_base + MBOX1_STATUS) & MBOX_FULL);
  
  hal_mmio_write(mbox_base + MBOX1_WRITE,
             (data & 0xfffffff0) | (uint32_t)(channel & 0xf));
}


/*
 *
 */
uint32_t hal_mbox_status(void)
{
  return hal_mmio_read(mbox_base + MBOX_STATUS);
}


/*
 *
 */
uint32_t *hal_mbox_find_tag(uint32_t tag, uint32_t *msg)
{
  uint32_t skip_size;

  msg += 2;

  while (*msg != 0 && *msg != tag) {
    skip_size = (*(msg + 1) + 12) / 4;
    msg += skip_size;
  }

  if (*msg == 0) {
    return NULL;
  }
  
  return msg;
}


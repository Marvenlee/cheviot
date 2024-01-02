/* Copyright (C) 2013 by John Cronin <jncronin@tysos.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "arm.h"
#include "mbox.h"
#include "mmio.h"
#include <stdint.h>

#define MBOX_FULL 0x80000000
#define MBOX_EMPTY 0x40000000


/*!
*/
void MailBoxWrite(uint32_t v, uint32_t id)
{
  uint32_t s;

  MemoryBarrier();
  DataCacheFlush();

  while (1) {
    s = MailBoxStatus();

    if ((s & 0x80000000) == 0)
      break;
  }

  MemoryBarrier();
  *((uint32_t *)(0x2000b8a0)) = v | id;
  MemoryBarrier();
}

/*!
*/
uint32_t MailBoxRead(uint32_t id)
{
  uint32_t s;
  volatile uint32_t v;

  MemoryBarrier();
  DataCacheFlush();

  while (1) {
    while (1) {
      s = MailBoxStatus();
      if ((s & 0x40000000) == 0)
        break;
    }

    MemoryBarrier();

    v = *((uint32_t *)(0x2000b880));

    if ((v & 0x0000000f) == id)
      break;
  }
  return v & 0xfffffff0;
}


/*
*/
uint32_t MailBoxStatus()
{
  volatile uint32_t v;
  MemoryBarrier();
  DataCacheFlush();
  v = *((uint32_t *)(0x2000b898));
  MemoryBarrier();
  return v;
}


/*
*/
uint32_t *FindTag(uint32_t tag, uint32_t *msg) {
  uint32_t skip_size;

  msg += 2;

  while (*msg != 0 && *msg != tag) {
    skip_size = (*(msg + 1) + 12) / 4;
    msg += skip_size;
  }

  if (*msg == 0)
    return NULL;

  return msg;
}


/*
 *
 */
uint32_t mbox_read(uint8_t channel)
{
  while (1) {
    while (mmio_read(MBOX_BASE + MBOX_STATUS) & MBOX_EMPTY) {
    }    

    uint32_t data = mmio_read(MBOX_BASE + MBOX_READ);
    uint8_t read_channel = (uint8_t)(data & 0xf);
    if (read_channel == channel) {
      return (data & 0xfffffff0);
    }
  }
}


/*
 *
 */
void mbox_write(uint8_t channel, uint32_t data)
{
  while (mmio_read(MBOX_BASE + MBOX_STATUS) & MBOX_FULL) {
  }
  
  mmio_write(MBOX_BASE + MBOX_WRITE,
             (data & 0xfffffff0) | (uint32_t)(channel & 0xf));
}


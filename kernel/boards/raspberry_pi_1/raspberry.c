/*
 * Copyright 2014  Marven Gilhespie
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

#include <kernel/arch.h>
#include <kernel/board/arm.h>
#include <kernel/board/globals.h>
#include <kernel/globals.h>

static uint32_t mailbuffer[64] __attribute__((aligned(16)));
static int led_state = 0;

/*
 *
 */
void MailBoxWrite(uint32_t v, uint32_t id) {
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

/*
 *
 */
uint32_t MailBoxRead(uint32_t id) {
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
 *
 */
uint32_t MailBoxStatus() {
  volatile uint32_t v;
  MemoryBarrier();
  DataCacheFlush();
  v = *((uint32_t *)(0x2000b898));
  MemoryBarrier();
  return v;
}

/*
 *
 */
void BlinkError(void) {
  int c;

  while (1) {
    LedOn();

    for (c = 0; c < 1600000; c++)
      ;

    LedOff();

    for (c = 0; c < 800000; c++)
      ;
  }
}

/*
 *
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
void BlankScreen(void) {
  uint32_t result;

  mailbuffer[0] = 7 * 4;
  mailbuffer[1] = 0;
  mailbuffer[2] = 0x00040002;
  mailbuffer[3] = 4;
  mailbuffer[4] = 0;
  mailbuffer[5] = 0;
  mailbuffer[6] = 0;

  do {
    MailBoxWrite((uint32_t)mailbuffer, 8);
    result = MailBoxRead(8);
  } while (result == 0);
}

/*
 *
 */
void GetScreenDimensions(void) {
  uint32_t result;

  screen_width = 640;
  screen_height = 480;

  mailbuffer[0] = 8 * 4;
  mailbuffer[1] = 0;
  mailbuffer[2] = 0x00040003;
  mailbuffer[3] = 8;
  mailbuffer[4] = 8;
  mailbuffer[5] = 0;
  mailbuffer[6] = 0;
  mailbuffer[7] = 0;

  do {
    MailBoxWrite((uint32_t)mailbuffer, 8);
    result = MailBoxRead(8);
  } while (result == 0);

  screen_width = mailbuffer[5];
  screen_height = mailbuffer[6];
}

/*
 *
 */
void SetScreenMode(void) {
  uint32_t result;
  uint32_t *tva;

  mailbuffer[0] = 22 * 4;
  mailbuffer[1] = 0;
  mailbuffer[2] = 0x00048003; // Physical size
  mailbuffer[3] = 8;
  mailbuffer[4] = 8;
  mailbuffer[5] = screen_width;
  mailbuffer[6] = screen_height;
  mailbuffer[7] = 0x00048004; // Virtual size
  mailbuffer[8] = 8;
  mailbuffer[9] = 8;
  mailbuffer[10] = screen_width;
  mailbuffer[11] = screen_height;
  mailbuffer[12] = 0x00048005; // Depth
  mailbuffer[13] = 4;
  mailbuffer[14] = 4;
  mailbuffer[15] = 16;         // 16 bpp
  mailbuffer[16] = 0x00040001; // Allocate buffer
  mailbuffer[17] = 8;
  mailbuffer[18] = 8;
  mailbuffer[19] = 16; // alignment
  mailbuffer[20] = 0;
  mailbuffer[21] = 0;

  do {
    MailBoxWrite((uint32_t)mailbuffer, 8);
    result = MailBoxRead(8);
  } while (result == 0);

  if (mailbuffer[1] != 0x80000000)
    BlinkError();

  tva = FindTag(0x00040001, mailbuffer);

  if (tva == NULL)
    BlinkError();

  screen_buf = (void *)*(tva + 3);

  mailbuffer[0] = 8 * 4;
  mailbuffer[1] = 0;
  mailbuffer[2] = 0x00040008; // Get Pitch
  mailbuffer[3] = 4;
  mailbuffer[4] = 0;
  mailbuffer[5] = 0;
  mailbuffer[6] = 0; // End Tag
  mailbuffer[7] = 0;

  do {
    MailBoxWrite((uint32_t)mailbuffer, 8);
    result = MailBoxRead(8);
  } while (result == 0);

  tva = FindTag(0x00040008, mailbuffer);

  if (tva == NULL)
    BlinkError();

  screen_pitch = *(tva + 3);
}

/*
 *
 */
uint16_t CalcColor(uint8_t r, uint8_t g, uint8_t b) {
  uint16_t c;

  b >>= 5;
  g >>= 5;
  r >>= 5;
  c = (r << 11) | (g << 5) | (b);
  return c;
}

/*
 *
 */
void PutPixel(int x, int y, uint16_t color) {
  *(uint16_t *)((uint8_t *)screen_buf + y * (screen_pitch) + x * 2) = color;
}

/*
 *
 */
void LedOn(void) {
  dmb();
  gpio_regs->fsel[1] = (1 << 18);
  gpio_regs->clr[0] = (1 << 16);
  led_state = 1;
  dmb();
}

/*
 *
 */
void LedOff(void) {
  dmb();
  gpio_regs->fsel[1] = (1 << 18);
  gpio_regs->set[0] = (1 << 16);
  led_state = 0;
  dmb();
}


/*
 *
 */
void LedToggle(void) {
  dmb();

  if (led_state == 1) {
    LedOff();
  } else {
    LedOn();
  }
}


/*!
 */
void BlinkLEDs(int cnt) {
  int t;
  static volatile uint32_t tim;

  for (t = 0; t < cnt; t++) {
    for (tim = 0; tim < 500000; tim++)
      ;

    LedOn();

    for (tim = 0; tim < 500000; tim++)
      ;

    LedOff();
  }
}

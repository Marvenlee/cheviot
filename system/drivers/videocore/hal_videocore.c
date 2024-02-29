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
#include <stddef.h>

// Mailbuffer for videocore.
// TODO: Can we set the SDCARD MB_ADDR in a similar way?
//       Perhaps add 0x40000000 offset to get the L2 Cache Coherent address?
static uint32_t mailbuffer[64] __attribute__((aligned(16)));
static uint16_t *screen_buf;
static uint32_t screen_pitch;

/*
*/
void hal_vc_get_screen_dimensions(uint32_t *screen_width, uint32_t *screen_height)
{
  uint32_t result;

  mailbuffer[0] = 8 * 4;
  mailbuffer[1] = 0;
  mailbuffer[2] = 0x00040003;
  mailbuffer[3] = 8;
  mailbuffer[4] = 8;
  mailbuffer[5] = 0;
  mailbuffer[6] = 0;
  mailbuffer[7] = 0;

  do {
    hal_mbox_write(MBOX_PROP, (uint32_t)mailbuffer);
    result = hal_mbox_read(MBOX_PROP);
  } while (result == 0);

  *screen_width = mailbuffer[5];
  *screen_height = mailbuffer[6];
}

/*!
*/
void hal_vc_set_screen_mode(uint32_t screen_width, uint32_t screen_height)
{
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
  mailbuffer[12] = 0x00048005; // Depth  FIXME: How does this match bpp below?
  mailbuffer[13] = 4;
  mailbuffer[14] = 4;
  mailbuffer[15] = 16;         // 16 Bits Per Pixel
  mailbuffer[16] = 0x00040001; // Allocate buffer
  mailbuffer[17] = 8;
  mailbuffer[18] = 8;
  mailbuffer[19] = 16; // alignment
  mailbuffer[20] = 0;
  mailbuffer[21] = 0;

  do {
    hal_mbox_write(MBOX_PROP, (uint32_t)mailbuffer);
    result = hal_mbox_read(MBOX_PROP);
  } while (result == 0);

  if (mailbuffer[1] != 0x80000000) {
    hal_blink_error();
  }
  
  tva = hal_mbox_find_tag(0x00040001, mailbuffer);

  if (tva == NULL)
    hal_blink_error();

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
    hal_mbox_write(MBOX_PROP, (uint32_t)mailbuffer);
    result = hal_mbox_read(MBOX_PROP);
  } while (result == 0);

  tva = hal_mbox_find_tag(0x00040008, mailbuffer);

  if (tva == NULL)
    hal_blink_error();

  screen_pitch = *(tva + 3);
}


/*
*/
void hal_vc_blank_screen(void)
{
  uint32_t result;

  mailbuffer[0] = 7 * 4;
  mailbuffer[1] = 0;
  mailbuffer[2] = 0x00040002;
  mailbuffer[3] = 4;
  mailbuffer[4] = 0;
  mailbuffer[5] = 0;
  mailbuffer[6] = 0;

  do {
    hal_mbox_write(MBOX_PROP, (uint32_t)mailbuffer);
    result = hal_mbox_read(MBOX_PROP);
  } while (result == 0);
}


/*
*/
uint16_t hal_vc_calc_rgb16(uint8_t r, uint8_t g, uint8_t b)
{
  uint16_t c;

  b >>= 5;
  g >>= 5;
  r >>= 5;

  c = (r << 11) | (g << 5) | (b);
  return c;
}


/*
*/
void hal_vc_putpixel(int x, int y, uint16_t color)
{
  *(uint16_t *)((uintptr_t)screen_buf + y * (screen_pitch) + x * 2) = color;
}



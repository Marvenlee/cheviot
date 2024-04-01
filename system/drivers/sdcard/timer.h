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

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include "sdcard.h"


struct timer_wait {
		struct timespec expire_ts;
    uint32_t timeout_usec;
};


extern struct timer_wait tw;


int delay_microsecs(int usec);
uint32_t read_microsecond_timer(void);
void register_timer(struct timer_wait * tw, unsigned int usec);
int compare_timer(struct timer_wait * tw);

/*
 * Macro to repeatedly poll a "stop_if_true" test until satisfied or the
 * timeout in microseconds elapses. This busy-waits, we could add code to 
 * yield to other tasks/processes.
 */
#define TIMEOUT_WAIT(stop_if_true, usec)                                       \
  do {                                                                         \
    register_timer(&tw, usec);                                                 \
    do {                                                                       \
      if (stop_if_true)                                                        \
        break;                                                                 \
    } while (!compare_timer(&tw));                                             \
  } while (0);

#endif


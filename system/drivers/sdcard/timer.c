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

#include <sys/debug.h>
#include "timer.h"
#include "globals.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include "sdcard.h"
#include <time.h>
#include <sys/time.h>
#include <machine/cheviot_hal.h>


struct timer_wait tw;


/*
 * This can put task to sleep, granularity is kernel's timer tick rate.
 * Unless we get fancy with variable intervals between timers.
 */
int delay_microsecs(int usec)
{
  struct timespec req, rem; 
  req.tv_sec = 0;
  req.tv_nsec = usec * 1000;
  
  while (nanosleep(&req, &rem) != 0) {
    req = rem;
  }
  
  return 0;
}


/*
 *
 */
void register_timer(struct timer_wait * tw, unsigned int usec)
{
	struct timespec timeout_ts;
	
	tw->timeout_nsec = usec * 1000;
	
	timeout_ts.tv_sec = 0;
	timeout_ts.tv_nsec = usec * 1000;
	
  clock_gettime(CLOCK_MONOTONIC_RAW, &tw->start_ts);
}


/*
 *
 */
int compare_timer(struct timer_wait * tw)
{
	struct timespec current_ts;
	struct timespec diff_ts;
	bool elapsed;
	
	clock_gettime(CLOCK_MONOTONIC_RAW, &current_ts);

	elapsed = diff_timespec(&diff_ts, &current_ts, &tw->start_ts);
	
	if (elapsed) {
		log_warn("timeout expired: %d s, %d usecs, timeout:%d", 
							(int)diff_ts.tv_sec, (int)diff_ts.tv_nsec / 1000, (int)tw->timeout_nsec);				
		return 1;
	} else {
		return 0;
	}
}


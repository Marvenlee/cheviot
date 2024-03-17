
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
#include <sys/rpi_gpio.h>
#include <errno.h>


/*
 *
 */
int rpi_configure_gpio(int pin, enum FSel fn, enum PullUpDown action)
{
	int sc;
	
	sc = _swi_rpi_configure_gpio(pin, fn, action);

	if (sc != 0) {
		errno = sc;
		return -1;
	}
	
	return 0;
}


/*
 *
 */
int rpi_set_gpio(int pin, int state)
{
	return _swi_rpi_set_gpio(pin, state);
}


/*
 *
 */
int rpi_get_gpio(int pin)
{
	return _swi_get_gpio(pin);
}



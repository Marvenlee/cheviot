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

#ifndef SYS_RPI_GPIO_H
#define SYS_RPI_GPIO_H

#include <stdint.h>
#include <machine/cheviot_hal.h>


/* GPIO Alternate Function Select (pin mux)
 */
enum FSel
{
    INPUT   = 0,
    OUTPUT  = 1,
    FN5     = 2,
    FN4     = 3,
    FN0     = 4,
    FN1     = 5,
    FN2     = 6,
    FN3     = 7,
};


/* GPIO pin pull-up configuration
 */
enum PullUpDown
{
    PULL_NONE = 0,
    PULL_UP   = 1,
    PULL_DOWN = 2
};


/*
 * Prototypes
 */

int rpi_configure_gpio(int pin, enum FSel fn, enum PullUpDown action);
int rpi_set_gpio(int pin, int state);
int rpi_get_gpio(int pin);


#endif

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
#include "gpio.h"

#define LED_PIN 16
#define LED_ON_STATE 0
#define LED_OFF_STATE 1


void init_led(void)
{
  configure_gpio(LED_PIN, OUTPUT, PULL_NONE);
}


/*
 */
void led_on(void)
{
  set_gpio(LED_PIN, LED_ON_STATE);
}


/*
 */
void led_off(void)
{
  set_gpio(LED_PIN, LED_OFF_STATE);
}


/*
 */
static void led_delay(int cnt)
{
  volatile int tmp;
  
  for (int i=0; i<cnt; i++) {
    tmp = i * i + i;
  } 
}

/*
 */
void blink_led(int cnt)
{
  int t;

  for (t = 0; t < cnt; t++) {
    led_on();
    led_delay(200000);

    led_off();
    led_delay(200000);
  }
}


/* @brief   Blink LEDs continuously to indicate error
 */
void blink_error(void)
{
  while (1) {
    led_on();
    led_delay(100000);

    led_off();
    led_delay(100000);
  }
}





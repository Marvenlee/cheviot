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
#include "aux_uart.h"
#include "debug.h"
#include "gpio.h"
#include "led.h"
#include "peripheral_base.h"


void board_init(void)
{
  hal_set_mbox_base((void *)MBOX_BASE);

  gpio_regs = (struct bcm2711_gpio_registers *)GPIO_BASE;
  aux_regs = (struct bcm2711_aux_registers *)AUX_BASE;

  init_aux_uart();
  
  aux_uart_write_byte('\n');
}

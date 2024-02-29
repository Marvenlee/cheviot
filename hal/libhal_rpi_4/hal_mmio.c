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


/* @brief   Write to a peripheral register
 *
 */
void hal_mmio_write(void *reg, uint32_t data)
{
  hal_dsb();
  *(volatile uint32_t *)(reg) = data;
  hal_dsb();
}


/* @brief   Read from a peripheral register
 *
 */
uint32_t hal_mmio_read(void *reg)
{
  uint32_t val;
  hal_dsb();
  val = *(volatile uint32_t *)(reg);
  hal_dsb();
  
  return val;
}




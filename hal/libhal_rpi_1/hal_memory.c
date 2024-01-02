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

#include <machine/raspberry_pi_hal.h>


/*
 */
__attribute__((naked)) void hal_memory_barrier(void)
{
  __asm("mov r0, #0");
  __asm("mcr p15, #0, r0, c7, c10, #5");
  __asm("mov pc, lr");
}


/*
 */
__attribute__((naked)) void hal_data_cache_flush(void)
{
  __asm("mov r0, #0");
  __asm("mcr p15, #0, r0, c7, c14, #0");
  __asm("mov pc, lr");
}


/*
 */
__attribute__((naked)) void hal_synchronisation_barrier(void)
{
  __asm("mov r0, #0");
  __asm("mcr p15, #0, r0, c7, c10, #4");
  __asm("mov pc, lr");
}


/*
 */
__attribute__((naked)) void hal_data_synchronisation_barrier(void)
{
  __asm("stmfd sp!, {r0-r8,r12,lr}");
  __asm("mcr p15, #0, ip, c7, c5, #0");
  __asm("mcr p15, #0, ip, c7, c5, #6");
  __asm("mcr p15, #0, ip, c7, c10, #4");
  __asm("mcr p15, #0, ip, c7, c10, #4");
  __asm("ldmfd sp!, {r0-r8,r12,pc}");
}



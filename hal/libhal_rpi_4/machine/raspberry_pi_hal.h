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

#ifndef SYS_RASPBERRY_PI_HAL_H
#define SYS_RASPBERRY_PI_HAL_H

#include <stdint.h>
#include <stdbool.h>


#if defined(RASPBERRY_PI_1)

#include <machine/board/raspberry_pi_1/hal_aux.h>
#include <machine/board/raspberry_pi_1/hal_interrupt.h>
#include <machine/board/raspberry_pi_1/hal_mailbox.h>
#include <machine/board/raspberry_pi_1/hal_pl011_uart.h>
#include <machine/board/raspberry_pi_1/hal_emmc.h>
#include <machine/board/raspberry_pi_1/hal_timer.h>


#elif defined(RASPBERRY_PI_4)

#include <machine/board/raspberry_pi_4/hal_aux.h>
#include <machine/board/raspberry_pi_4/hal_interrupt.h>
#include <machine/board/raspberry_pi_4/hal_mailbox.h>
#include <machine/board/raspberry_pi_4/hal_pl011_uart.h>
#include <machine/board/raspberry_pi_4/hal_emmc.h>
#include <machine/board/raspberry_pi_4/hal_timer.h>


#else
#error "No Raspberry Pi model defined"
#endif

#endif

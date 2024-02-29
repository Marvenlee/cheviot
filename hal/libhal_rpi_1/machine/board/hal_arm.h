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

#ifndef MACHINE_BOARD_HAL_ARM_H
#define MACHINE_BOARD_HAL_ARM_H

#include <stdint.h>
#include <sys/types.h>


/*
 * types
 */
typedef volatile int spinlock_t;
typedef uint32_t vm_addr;
typedef uint32_t vm_offset;
typedef uint32_t vm_size;
typedef uint32_t pte_t;
typedef uint32_t pde_t;


uint32_t hal_get_cpsr(void);
uint32_t hal_get_spsr(void);
vm_addr hal_get_far(void);
uint32_t hal_get_dfsr(void);
uint32_t hal_get_ifsr(void);
vm_addr hal_get_vbar(void);
void hal_set_vbar(vm_addr va);
uint32_t hal_get_dacr(void);
void hal_set_dacr(uint32_t dacr);
uint32_t hal_get_ctrl(void);
void hal_set_ctrl(uint32_t reg);

void hal_set_page_directory(void *phys_pd);
void hal_flush_all_caches(void);
void hal_flush_caches(void);
void hal_invalidate_tlb(void);
void hal_enable_l1_cache(void);
void hal_disable_l1_cache(void);
void hal_enable_paging(vm_addr pagedir, uint32_t flags);

void hal_isb(void);
void hal_memory_barrier(void);
void hal_data_cache_flush(void);
void hal_sync_barrier(void);
void hal_data_sync_barrier(void);



#endif

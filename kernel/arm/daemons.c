/*
 * Copyright 2014  Marven Gilhespie
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
 
/*
 * Kernel background tasks
 */

#include <kernel/types.h>
#include <kernel/vm.h>
#include <kernel/proc.h>
#include <kernel/dbg.h>
#include <kernel/utility.h>
#include <kernel/arch.h>
#include <kernel/arm/init.h>
#include <kernel/globals.h>




/*
 * The idle-loop of idle-processes.
 * Called when there is no other process running on a CPU.
 */

void idle_loop (void)
{
    while (1)
    {
    }
}





/*
 * Unwritten.  Periodically makes system calls to coalesce the free areas
 * in the virtual address space and physical address space.  Compacts
 * physical memory, moving all segments towards the bottom.
 *
 */
 
void CompactionDaemon (void)
{
    while (1)
    {
    }
}
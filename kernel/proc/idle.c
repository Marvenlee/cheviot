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
 * Helper processes that run inside the kernel.  As the kernel uses a single
 * kernel stack (interrupt model kernel) these tasks are called as continuations
 * within the KernelExit path.  If these functions are preempted they are
 * restarted from the beginning.
 *
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
 * Idle Task, continuation called from within KernelExit
 * The idle-loop for each CPU's idle process.
 */

void IdleTask (void)
{
    EnablePreemption();
    
    while (1)
    {
    }
}



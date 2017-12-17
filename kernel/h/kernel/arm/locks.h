#ifndef KERNEL_ARM_LOCKS_H
#define KERNEL_ARM_LOCKS_H

#include <kernel/types.h>
#include <kernel/arm/arm.h>


extern void Reschedule(void);



/*
 * Prototypes
 */

void DisablePreemption(void);
void EnablePreemption(void);







#endif

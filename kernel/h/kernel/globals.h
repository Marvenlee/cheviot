#ifndef KERNEL_GLOBALS_H
#define KERNEL_GLOBALS_H

#define ARCH_ARM


#ifdef ARCH_I386
#include <kernel/i386/globals.h>
#endif

#ifdef ARCH_ARM
#include <kernel/arm/globals.h>
#endif







#endif


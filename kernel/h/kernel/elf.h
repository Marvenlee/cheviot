#ifndef KERNEL_ELF_H
#define KERNEL_ELF_H

#define ARCH_ARM

#ifdef ARCH_I386
#endif

#ifdef ARCH_ARM
#include <kernel/arm/elf.h>
#endif

#endif

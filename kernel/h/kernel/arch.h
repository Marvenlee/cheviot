#ifndef KERNEL_ARCH_H
#define KERNEL_ARCH_H

#define ARCH_ARM

#ifdef ARCH_I386
#include <kernel/i386/i386.h>
#include <kernel/i386/task.h>
#endif

#ifdef ARCH_ARM
#include <kernel/board/arm.h>
#include <kernel/board/task.h>
#endif

#endif

#ifndef KERNEL_DBG_H
#define KERNEL_DBG_H

#include <kernel/arch.h>
#include <kernel/proc.h>
#include <kernel/types.h>
#include <kernel/vm.h>
#include <stdarg.h>


/*
 * At end of most debug statements semi-colons aren't needed or will
 * cause problems compiling.
 *
 * Multi-statement Debug macros have braces surrounding them in case
 * they're used like below...
 *
 * if (condition)
 *     KPANIC (str);
 *
 *
 * Use -DNDEBUG in the makefile CFLAGS to remove debugging.
 * Use -DDEBUG_LEVEL=x to set debugging level.
 * Or define DEBUG_LEVEL at top of source file.
 */

#define NDEBUG

#ifdef NDEBUG
#undef KDEBUG
#endif

#ifdef KDEBUG

#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 2
#endif

#define Error(fmt, args...) DoLog(fmt, ##args)

#if DEBUG_LEVEL >= 1
#define Warn(fmt, args...) DoLog(fmt, ##args)
#else
#define Warn(fmt, args...)
#endif

#if DEBUG_LEVEL >= 2
#define Info(fmt, args...) DoLog(fmt, ##args)
#else
#define Info(fmt, args...)
#endif

#if DEBUG_LEVEL >= 3
#define KLog(fmt, args...) DoLog(fmt, ##args)
#else
#define KLog(fmt, args...)
#endif

#define KASSERT(expr)                                                          \
  {                                                                            \
    if (!(expr)) {                                                             \
      PrintKernelPanic(#expr ", %s, %d, %s", __FILE__, __LINE__, __FUNCTION__);\
    }                                                                          \
  }


#define KernelPanic()                                                          \
  {                                                                            \
    DisableInterrupts();                                                       \
    PrintKernelPanic("panic, %s, %d, %s", __FILE__, __LINE__, __FUNCTION__);   \
  }


#else

#define Error(fmt, args...)
#define Warn(fmt, args...)
#define Info(fmt, args...)
#define KLog(fmt, args...)

#define KASSERT(expr)
#define KernelPanic()                                                          \
  {                                                                            \
    DisableInterrupts();                                                       \
    while(1);                                                                  \
  }

#endif


/*
 * TODO: Alow selective control of modules that enable debug
 */
#define DEBUG_INIT
#define DEBUG_ARCH

#define DEBUG_SCHED
#define DEBUG_PROC
#define DEBUG_TIMER

#define DEBUG_MOUNT
#define DEBUG_LOOKUP
#define DEBUG_CACHE
#define DEBUG_CHAR
#define DEBUG_BLK
#define DEBUG_PIPE
#define DEBUG_ACCESS
#define DEBUG_DIR
#define DEBUG_VFS
#define DEBUG_VNODE
#define DEBUG_FILE
#define DEBUG_OPEN
#define DEBUG_HANDLE
#define DEBUG_POLL




/*
 * Kernel debugging functions.  Shouldn't be called directly, instead use the
 * macros above.
 */

SYSCALL void SysDebug(char *s);

void DoLog(const char *format, ...);
void KLog2(const char *format, va_list ap);
void PrintKernelPanic(char *format, ...);
void KLogToScreenDisable(void);
void KPrintXY(int x, int y, char *s);

void ProcessesInitialized(void);

void MemDump(void *addr, size_t sz);

#endif

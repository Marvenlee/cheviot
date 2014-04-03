#ifndef DBG_H
#define DBG_H


#include "types.h"
#include "arm.h"
#include <stdarg.h>

//#define NDEBUG

extern struct Process *root_process;

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
 */


#ifndef NDEBUG


#define KLOG( fmt, args...)												\
		KLog (fmt , ##args)


#define KPRINTF( fmt, args...)											\
		KLog (fmt , ##args);											


#define KTRACE														\
		KLog ("E: %s", __FUNCTION__);


#define KLOG_LINE														\
		KLog ("Line: %s, %d, %s", __FILE__, __LINE__, __FUNCTION__);


#define KRETURN( rc)													\
		while(1)														\
		{																\
		KLog ("L: %s, rc = %d", __FUNCTION__, (int)rc); 				\
		return rc;														\
		}

		
#define KASSERT(expr)													\
		{                                                               \
			if (!(expr))												\
			{															\
				KLog("@ %s, %d, %s",__FILE__, __LINE__, __FUNCTION__);	\
				KLog("KASSERT (" #expr  ") failed");					\
				KPanic();												\
			}                                                           \
		}

/* FIXME, alter or remove KPANIC */
#define KPANIC(str)														\
		{																\
			struct Process *dcurrent;						\
			dcurrent = GetCurrentProcess();								\
			DisableInterrupts();										\
			KLog("@ %s, %d, %s, %s",__FILE__, __LINE__, __FUNCTION__, dcurrent->name);		\
			KPanic(str);                                                   \
		}




#else	

#define KPRINTF( fmt, args...)
#define KLOG( fmt, args...)
#define KTRACE
#define KLOG_PLACE
#define KLOG_LEAVE
#define KASSERT( expr)
#define KPANIC( str)		KPanic (str);


#endif




/*
 * Kernel debugging functions.  Shouldn't be called directly, instead use the
 * macros above.
 */
void dbg_init(void);
void KLog (const char *format, ...);
void KLog2 (const char *format, va_list ap);
void KPanic(char *str);
void KLogToScreenDisable(void);




#endif

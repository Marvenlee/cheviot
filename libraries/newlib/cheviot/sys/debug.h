#ifndef _SYS_DEBUG_H
#define _SYS_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif


#include <sys/syscalls.h>
#include <stdlib.h>
#include <stdio.h>

extern bool __debug_enabled;

extern char __assertion_string[32];


void KLogOut (const char *format, ...);



#ifndef NDEBUG

#define KLog( fmt, args...)												\
		KLogOut (fmt , ##args)

#define KAssert(expr)														\
		{                                           	                    \
			if (!(expr))													\
			{																\
				KLogOut("@ %s, %d, %s",__FILE__, __LINE__, __FUNCTION__);	\
				KLogOut("KASSERT (" #expr  ") failed");						\
				snprintf (__assertion_string, 32,							\
						"@ %s, %d, %s",__FILE__, __LINE__, __FUNCTION__);	\
				Exit(EXIT_ASSERTION, __assertion_string);					\
			}                                                           	\
		}

#else 

#define KLog( fmt, args...)

#define KAssert(expr)


#endif




#ifdef __cplusplus
}
#endif
#endif



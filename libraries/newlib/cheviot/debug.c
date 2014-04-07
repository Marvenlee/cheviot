#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/debug.h>
#include <stdarg.h>



static char __klog_buf[80];

void KLogOut (const char *format, ...)
{
	va_list ap;
	
	va_start (ap, format);
	
	if (__debug_enabled)
	{
		vsnprintf (__klog_buf, 79, format, ap);
		Debug (__klog_buf);
	}
	
	va_end (ap);
}


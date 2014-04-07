#include <errno.h>
#include <sys/types.h>
#include <sys/syscalls.h>
#include <sys/time.h>
#include <time.h>
#include <utime.h>




/*
 *
 */

time_t time (time_t *tloc)
{
	struct timeval tv;
	
	gettimeofday (&tv, NULL);
	
	if (tloc != NULL)
		*tloc = tv.tv_sec;
	
	return tv.tv_sec;
}




/*
 *
 */

int utime(const char *path, const struct utimbuf *times)
{
	return 0;
}





/*
 *
 */

int gettimeofday (struct timeval *tp, void *tzp)
{
	struct TimeVal tv;
	int result;
		
	result = GetTimeOfDay (&tv);
	tp->tv_sec = tv.seconds;
	tp->tv_usec = tv.microseconds;
	return result;
}




/*
 *
 */

int settimeofday(const struct timeval *tp, const struct timezone *tzp)
{
	struct TimeVal tv;
	
	tv.seconds = tp->tv_sec;
	tv.microseconds = tp->tv_usec;

	return SetTimeOfDay (&tv);
}










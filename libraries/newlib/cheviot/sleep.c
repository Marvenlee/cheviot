#include <errno.h>
#include <sys/types.h>
#include <sys/syscalls.h>



static int __timer = -1;

int Sleep (int seconds)
{
	struct TimeVal tv;

	tv.seconds = seconds;
	tv.microseconds = 0;

	if (__timer == -1)
	{
		if ((__timer = CreateTimer()) == -1)
			return -1;
	}
	
		
	if (SetTimer (__timer, TIMER_TYPE_RELATIVE, &tv) == 0)
	{
		WaitFor (__timer);
		return 0;
	}
	
	return -1;
}


int USleep (int usec)
{
	struct TimeVal tv;

	tv.seconds = 0;
	tv.microseconds = usec;

	if (__timer == -1)
	{
		if ((__timer = CreateTimer()) == -1)
			return -1;
	}
	
		
	if (SetTimer (__timer, TIMER_TYPE_RELATIVE, &tv) == 0)
	{
		WaitFor (__timer);
		return 0;
	}
	
	return -1;
}


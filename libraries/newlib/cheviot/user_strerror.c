#include <_ansi.h>
#include <sys/syscalls.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>



char * _user_strerror (int errnum)
{
	char *error;
	
	switch (errnum)
	{
		default:
			error = NULL;
	}
	
	return error;		
}



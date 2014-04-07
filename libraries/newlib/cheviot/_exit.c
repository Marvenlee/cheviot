#include <limits.h>
#include <_ansi.h>
#include <_syslist.h>
#include <sys/syscalls.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/debug.h>




/*
 * Exit, flushing stdio buffers if necessary.
 */

void _exit (int rc)
{
	Exit (rc);
	while (1);
}

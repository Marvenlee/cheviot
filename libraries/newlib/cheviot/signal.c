#include <_ansi.h>
#include <_syslist.h>
#include <sys/syscalls.h>
#include <errno.h>
#include <signal.h>




int raise (int sig)
{
	Exit (EXIT_FATAL);
}


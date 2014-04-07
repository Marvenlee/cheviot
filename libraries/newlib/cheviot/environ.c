#include <sys/syscalls.h>
#include <sys/debug.h>
#include <interfaces/filesystem.h>
#include <errno.h>

char *__env[1] = { 0 }; 
char **environ = __env;


bool __debug_enabled = FALSE;

// port_error_handler_t port_error_handler[NHANDLE];


int __argc = 0;
char **__argv;
int __envc = 0;




void __set_errno (int err)
{
	errno = err;
}



int _isatty (int fd)
{
    return 1;
}
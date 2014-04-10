#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/syscalls.h>
#include <sys/debug.h>





// **************************************************************************
// SizeOfAlloc() returns the actual size of memory, rounded up to the page
// size that will be allocated for a requested size.
// **************************************************************************

// Memory management alignment macros

#define ALIGN_UP(val, alignment)                                \
    ((((val) + (alignment) - 1)/(alignment))*(alignment))
#define ALIGN_DOWN(val, alignment)                              \
            ((val) - ((val) % (alignment)))



size_t SizeOfAlloc (size_t len)
{
    return ALIGN_UP (len, PAGE_SIZE);
}













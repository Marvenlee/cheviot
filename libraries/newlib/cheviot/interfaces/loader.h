#ifndef KERNEL_LOADER_H
#define KERNEL_LOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/syscalls.h>
#include <interfaces/filesystem.h>


// FIXME: Move into syscalls.h


struct MsgSpawn
{
	int cmd;
	int error;
	int result;
	bits32_t flags;
	int argc;
	int envc;
	int portc;
};



#ifdef __cplusplus
}
#endif

#endif

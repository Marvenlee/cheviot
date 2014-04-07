#ifndef INTERFACES_BENCHMARK_H
#define INTERFACES_BENCHMARK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/syscalls.h>
#include <interfaces/port.h>


struct MsgBench
{
	int cmd;
	int len;
};





#ifdef __cplusplus
}
#endif

#endif

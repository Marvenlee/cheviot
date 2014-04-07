#ifndef _SYS_KSYSCALLS_H
#define _SYS_KSYSCALLS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <machine/i386.h>



/*
 * Scheduling policies
 */

#define SCHED_OTHER						0
#define SCHED_RR						1
#define SCHED_FIFO						2



/*
 * Exit()/Join() Status
 */

#define EXIT_OK			0
#define EXIT_ERROR		1
#define EXIT_FATAL		2
#define EXIT_KILLED		3



/*
 * MsgSetErrorHandler() default handlers
 */
 
typedef void (*port_error_handler_t)(int handle);

#define PORT_ERRORS_ARE_FATAL      	((port_error_handler_t)NULL)
#define PORT_ERRORS_RETURN		  	((port_error_handler_t)-1)




typedef struct 
{
	bits32_t flags;
	int process_cnt;
	int cpu_cnt;
	int total_pages;
	int page_size;
	int virtualalloc_alignment;
	int max_memareas;
	int max_handles;
	int power_state;
	int max_tickets;
} sysinfo_t;




/*
 * Time related structures
 */


struct TimeVal
{
	long long seconds;
	long microseconds;
};





/*
 * Timer types, currently PERIODIC is not implemented
 */

#define TIMER_TYPE_RELATIVE				0
#define TIMER_TYPE_ABSOLUTE				1
#define TIMER_TYPE_PERIODIC				2




/*
 * VirtualAlloc() protections
 */

#define PROT_NONE			0
#define PROT_READ			(1<<0)
#define PROT_WRITE			(1<<1)
#define PROT_EXEC			(1<<2)
#define PROT_ALL			(PROT_READ | PROT_WRITE | PROT_EXEC)
#define PROT_READWRITE 		(PROT_READ | PROT_WRITE)
#define PROT_READEXEC		(PROT_READ | PROT_EXEC)

#define MAP_FIXED				(1<<3)
#define MAP_WIRED				(1<<4)
#define MAP_NOX64				(1<<5)
#define MAP_BELOW16M			(1<<6)
#define MAP_BELOW4G				(1<<7)

#define CACHE_DEFAULT	 		(0<<8)
#define CACHE_WRITEBACK	 		(1<<8)
#define CACHE_WRITETHRU	 		(2<<8)
#define CACHE_WRITECOMBINE 		(3<<8)
#define CACHE_UNCACHEABLE  		(4<<8)
#define CACHE_WEAKUNCACHEABLE	(5<<8)
#define CACHE_WRITEPROTECT		(6<<8)


#define PROT_MASK				0x00000007
#define CACHE_MASK   			0x00000f00



/*
 *
 */

void Debug (char *str);

int Spawn (void);
void Exit (int status);
int Join (int pid);
int CloseHandle (int handle);
int WaitFor (int handle);
int CheckFor (int handle);
int SetSchedParams (int h, int policy, int priority);
void Yield (void);

int AddInterruptHandler (int irq);
int MaskInterrupt (int irq);
int UnmaskInterrupt (int irq);
int CreateTimer (void);
int SetTimer (int handle, int type, struct TimeVal *tv);
int GetSystemTime (struct TimeVal *tv);

int SystemInfo (sysinfo_t *si);

void *VirtualAlloc (void *addr, size_t size, bits32_t flags);
void *VirtualAllocPhys (void *addr, size_t size, bits32_t flags, void *phys_addr);
int VirtualFree (void *addr);
int VirtualProtect (void *addr, bits32_t flags);
size_t VirtualSizeOf (void *addr);
int VirtualSwap (void *addr1, void *addr2);
int VirtualWire (void *addr);
int VirtualUnwire (void *addr);

int PutMsg (int handle, void **iov, int iov_cnt);
ssize_t GetMsg (int handle, void **msg, bits32_t access);
int CreateChannel (int handle[2]);
int IsAChannel(int handle1, int handle2);

int PutCondition (int handle, int value);
int GetCondition (int handle);
int CreateCondition (int handle[2]);










#ifdef __cplusplus
}
#endif

#endif


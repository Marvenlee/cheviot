#ifndef _SYS_KSYSCALLS_H
#define _SYS_KSYSCALLS_H

#ifdef __cplusplus
extern "C" {
#endif

//#include <machine/i386.h>

typedef signed char 		int8;
typedef unsigned char 		uint8;

typedef signed short		int16;
typedef unsigned short		uint16;

typedef signed long			int32;
typedef unsigned long		uint32;

typedef signed long long 	int64;
typedef unsigned long long 	uint64;

typedef signed long			err32;

typedef uint32				vm_offset;
typedef uint32				vm_size;

//typedef unsigned long		signals_t;

typedef unsigned char		bits8_t;
typedef unsigned short		bits16_t;
typedef unsigned long		bits32_t;

typedef long long			uuid_t;

typedef unsigned char		bool;

/*
 * Array of handles passed to Spawn and retrieve by GetSystemPorts.
 */


#define SYSTEM_PORT         0
#define EXCEPTION_PORT      1
#define ROOT_DIR_PORT       2
#define PROGRAM_DIR_PORT    3
#define CURRENT_DIR_PORT    4
#define STDIN_PORT          5
#define STDOUT_PORT         6
#define STDERR_PORT         7    


/*
 * Spawn flags
 */

#define PROCF_EXECUTIVE         (1<<0) 
#define PROCF_ALLOW_IO          (1<<1)




/*
 * Array size of segment and handle fields in SpawnArgs
 */
 
#define NSPAWNSEGMENTS      32
#define NSYSPORT            10


/*
 *
 */

typedef struct SpawnArgs
{
    void *entry;
    void *stack_top;
    char **argv;
    int argc;
    char **envv;
    int envc;
    bits32_t flags;
    void *segment[NSPAWNSEGMENTS];
    int segment_cnt;
    int handle[NSYSPORT];
    int handle_cnt;    
} spawnargs_t;




/*
 * PutMsg() and PutHandle() flags
 */


#define MSG_GRANT_ONCE               (1<<0)
#define MSG_SILENT                   (1<<1)





/*
 * Scheduling policies for SetSchedParams();
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
 * System information
 */

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
 * Timer types
 */

#define TIMER_TYPE_RELATIVE				0
#define TIMER_TYPE_ABSOLUTE				1





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

int Spawn (spawnargs_t args);
void Exit (int status);
int Join (int pid);
int CloseHandle (int handle);
int WaitFor (int handle);
int CheckFor (int handle);
int SetSchedParams (int policy, int priority);
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
int VirtualWire (void *addr);
int VirtualUnwire (void *addr);

int PutMsg (int handle, void *msg, bits32_t flags);
ssize_t GetMsg (int handle, void **msg, bits32_t access);
int PutHandle (int port_handle, int handle_to_send, bits32_t flags);
int GetHandle (int port_handle, void **rcv_handle);
int CreateChannel (int handle[2]);
int IsAChannel(int handle1, int handle2);

int PutNotification (int handle, int value);
int GetNotification (int handle);
int CreateNotification (int handle[2]);










#ifdef __cplusplus
}
#endif

#endif


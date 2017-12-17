#ifndef KERNEL_PROC_H
#define KERNEL_PROC_H

#include <kernel/types.h>
#include <kernel/lists.h>
#include <kernel/vm.h>
#include <kernel/timer.h>
#include <kernel/arch.h>



/*
 * Forward declarations
 */
 
struct Process;


/*
 *
 */

typedef struct ProcessInfo
{
	int dummy;
} processinfo_t;




/*
 *
 */

typedef struct  SysInfo
{
    bits32_t flags;
  
    size_t page_size;
    size_t mem_alignment;
    
    int max_pseg;
    int pseg_cnt;
    int max_vseg;
    int vseg_cnt;

    size_t avail_mem_sz;
    size_t total_mem_sz;
    
    int max_process;
    int max_handle;
    int free_process_cnt;
    int free_handle_cnt;
    
    int cpu_cnt;
    int cpu_usage[MAX_CPU];  
  
    int power_state;

    int max_ticket;
    int default_ticket;
    
} sysinfo_t;







/*
 * Exit and Join status
 */

#define EXIT_SUCCESS    0
#define EXIT_FAILURE    1
#define EXIT_FATAL      2
#define EXIT_KILLED     3




/*
 * Process states
 */

#define PROC_STATE_UNALLOC              0
#define PROC_STATE_INIT                 100
#define PROC_STATE_ZOMBIE               200
#define PROC_STATE_RUNNING              300
#define PROC_STATE_READY                500
#define PROC_STATE_SLEEP                800




/*
 * Scheduling policy flags and kernel constants
 */


#define SCHED_OTHER                     0
#define SCHED_RR                        1
#define SCHED_FIFO                      2
#define SCHED_IDLE                      -1

 
#define STRIDE1                         1000000
#define STRIDE_DEFAULT_TICKETS          100
#define STRIDE_MAX_TICKETS              800
#define PROCESS_QUANTA                  2



/*
 * Kernel condition variable for Sleep()/Wakeup() calls
 */
 
struct Rendez
{
    LIST (Process) process_list;
};




/*
 * Interrupt Service Routine kernel object
 */

struct ISRHandler
{
    LIST_ENTRY (ISRHandler) isr_handler_entry;
    int irq;
    int handle;
};




/*
 * Handle types and flags
 */

#define HANDLE_TYPE_FREE                0
#define HANDLE_TYPE_PROCESS             1
#define HANDLE_TYPE_ISR                 2
#define HANDLE_TYPE_FILESYSTEM			3
#define HANDLE_TYPE_PIPE                4
#define HANDLE_TYPE_SOCKETPAIR          5
#define HANDLE_TYPE_TIMER               6




/*
 * struct Handle;
 */

struct Handle
{
    int type;                       // Type of kernel object handle points to
    int pending;                    // Event is pending
    struct Process *owner;          // Owner process or NULL if in message
    void *object;                   // Pointer to kernel object
    bits32_t flags;                 
    LIST_ENTRY (Handle) link;       // On free or process's pending list
    
    struct Parcel *parcel;          // Pointer to Parcel if being sent as message
};





#define PROCF_KERNELTASK	(1<<0)
#define PROCF_ALLOW_IO		(1<<1)

/*
 * struct Process;
 */

struct Process
{
    struct TaskState task_state;        // Arch-specific state
    int handle;                         // Handle ID for this process
	struct Process *parent;    
    
    CIRCLEQ_ENTRY (Process) sched_entry; // real-time run-queue
    LIST_ENTRY (Process) stride_entry;     
    
    int sched_policy;                   
    int state;                          // Process state
    int priority;                       // real-time task priority
    int quanta_used;                    
    int tickets;                        // Stride scheduler state
    int stride;
    int remaining;
    int64 pass;
    
    struct AddressSpace as;
//    struct Pmap *pmap;                  // Arch-Specific page tables
    
    bits32_t flags;                     // Spawn() flags
    
    struct TimeVal syscall_start_time;  // Time of start of syscall for VM watchdog purposes.
    int watchdog_state;
    
    
    
    size_t virtualalloc_size;             // Size of memory requested by virtualalloc
    bits32_t virtualalloc_flags;
    struct Segment *virtualalloc_segment;
    int virtualalloc_state;
    
    
    void (*continuation_function)(void); // Deferred procedure called from
                                         // KernelExit().
    
    union                               // Continuation/deferred procedure state
    {
        struct                          // VirtualAlloc memory cleansing state
        {
            vm_addr addr;
            bits32_t flags;
        } virtualalloc;
    } continuation;
    
    
    LIST_ENTRY (Process) alloc_link;
    
    
    LIST_ENTRY (Process) free_entry;
    
    int exit_status;                     // Exit() error code

    struct Rendez *sleeping_on;
    LIST_ENTRY (Process) rendez_link;
    
    struct Rendez waitfor_rendez;
    int waiting_for;
    LIST (Handle) pending_handle_list;
    LIST (Handle) close_handle_list;     // To be closed in KernelExit()
    
    int vm_retries;
};


typedef struct MsgInfo
{
	int client_pid;
} msginfo_t;


/*
 * Message passing link between two handles
 * CONSIDER: Split into two Port structures pointing to each other.
 */
 
struct Port
{
	bits32_t flags;
	int handle;
	struct Process *owner;
	
	LIST_ENTRY (Process) connection_list;
};

 
struct Channel
{
	struct Channel *dst;
	LIST_ENTRY (Channel) link;

//	struct MsgHdr msg_queue[NMSG];

// FIXME: How to reply to a message?   Do need unique handle for each channel?  rcv_id
// Could be handle of port used on client side?
// symmetric ??    putmsg/getmsg????

// Channels separate from ports?
	int handle;
	struct Process *owner;
};



/*
 * Typedefs for linked lists
 */

LIST_TYPE (Channel) channel_list_t;
LIST_TYPE (Port) port_list_t;
LIST_TYPE (Handle) handle_list_t;
LIST_TYPE (ISRHandler) isrhandler_list_t;
LIST_TYPE (Process) process_list_t;
CIRCLEQ_TYPE (Process) process_circleq_t;




/*
 * Function Prototypes
 */


// event.c

SYSCALL int CheckEvent (int handle);
SYSCALL int WaitEvent (int handle);
void DoRaiseEvent (int handle);
void DoClearEvent (struct Process *proc, int handle);


// handle.c

SYSCALL int CloseHandle (int handle);
void ClosePendingHandles (void);
void *GetObject (struct Process *proc, int handle, int type);
void SetObject (struct Process *proc, int handle, int type, void *object);
struct Handle *FindHandle (struct Process *proc, int h);
int PeekHandle (int index);
int AllocHandle (void);
void FreeHandle (int handle);


// file.c

struct stat
{
    int dummy;
};


SYSCALL int mount (int dir_fd, int mountpoint_fd, char *name);
SYSCALL int unmount (int dir_fd, char *name);
SYSCALL int CreatePipe (int fd[2]);
SYSCALL int CreateFilesystem (int fd[2]);
SYSCALL int CreateSocketpair (int fd[2]);
SYSCALL ssize_t Ioctl (int fd, void *buf, size_t sz, void *reply_buf, size_t reply_sz);
SYSCALL int Write (int fd, void *msg, size_t sz);
SYSCALL ssize_t Read (int fd, void *buf, size_t sz);
SYSCALL ssize_t Seek (int fd, int whence, size_t pos);
SYSCALL ssize_t Readdir (int fd, void *buf, size_t sz);
SYSCALL int Symlink (int root_fd, char *path, char *linktopath);
SYSCALL int Authenticate (int mount_fd);
SYSCALL int Deauthenticate (int mount_fd);
SYSCALL int OpenAt (int dir_fd, char *pathname, int flags, mode_t mode);
SYSCALL int Stat (int fd, struct stat *stat);
SYSCALL int RemoveAt (int dir_fd, char *pathname);
SYSCALL int ChMod (int fd, mode_t mode);
int DoCloseFileHandle (int fd);


// info.c
 
SYSCALL int SystemInfo (sysinfo_t *si);


// interrupt.c

SYSCALL int CreateInterrupt (int irq);
int DoCloseInterruptHandler (int handle);
struct ISRHandler *AllocISRHandler(void);
void FreeISRHandler (struct ISRHandler *isr_handler);


// proc.c

SYSCALL int Fork (bits32_t flags);
SYSCALL int WaitPid (int pid, int *status);
SYSCALL void Exit (int status);
struct Process *AllocProcess (void);
void FreeProcess (struct Process *proc);
struct Process *GetProcess (int idx);
int DoCloseProcess (int handle);
void DoExit (int status);


// sched.c

void SchedLock (void);
void SchedUnlock (void);

void Reschedule (void);
void SchedReady (struct Process *proc);
void SchedUnready (struct Process *proc);
SYSCALL int ChangePriority (int32 priority);
void DisablePreemption(void);
void EnablePreemption(void);
void InitRendez (struct Rendez *rendez);
void Sleep (struct Rendez *rendez);
void Wakeup (struct Rendez *rendez);
void WakeupAll (struct Rendez *rendez);
void WakeupProcess (struct Process *proc);


// Architecture-specific 
             

int ArchForkProcess (struct Process *proc, struct Process *current);
void ArchFreeProcess (struct Process *proc);             
void SwitchTasks (struct TaskState *current, struct TaskState *next, struct CPU *cpu);
int MaskInterrupt (int irq);
int UnmaskInterrupt (int irq);




#endif


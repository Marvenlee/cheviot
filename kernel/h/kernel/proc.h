#ifndef KERNEL_PROC_H
#define KERNEL_PROC_H

#include <kernel/types.h>
#include <kernel/lists.h>
#include <kernel/vm.h>
#include <kernel/timer.h>
#include <kernel/arch.h>
#include <kernel/parcel.h>



/*
 * Forward declarations
 */
 
struct Process;


/*
 * Array of handles passed to Spawn and retrieve by GetSystemPorts.
 */


#define NSPAWNSEGMENTS     32       // Maximum segments to pass to Spawn




/*
 * Spawn flags
 */

#define PROCF_ALLOW_IO          (1<<0)      // Device driver IO
#define PROCF_FILESYS           (1<<1)      // Root / file system process
#define PROCF_DAEMON            (1<<2)      // Kernel Use: For kernel daemons

#define PROCF_SYSTEMMASK        (PROCF_FILESYS | PROCF_DAEMON)




/*
 *
 */

struct SpawnArgs
{
    void *entry;
    void *stack_top;
    char **argv;
    int argc;
    char **envv;
    int envc;
    bits32_t flags;
    int namespace_handle;
};




/*
 *
 */

typedef struct ProcessInfo
{
    int sighangup_handle;
    int sigterm_handle;
    int namespace_handle;
    
    char **argv;
    int argc;
    char **envv;
    int envc;

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
 * PutMsg() and PutHandle() flags
 */


#define MSG_GRANT_ONCE               (1<<0)         // Prevent forwarding handle
#define MSG_SILENT                   (1<<1)         // Do not raise event on this PutMsg()




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
#define HANDLE_TYPE_CHANNEL             4
#define HANDLE_TYPE_TIMER               5
#define HANDLE_TYPE_SYSTEMEVENT         6   // sighangup, sigterm, sigresource events


#define HANDLEF_GRANTED_ONCE            (1<<0)  // Handle has been granted once




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



  
    



/*
 * struct Process;
 */

struct Process
{
    struct TaskState task_state;        // Arch-specific state
    int handle;                         // Handle ID for this process
    
    
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
    
    struct Pmap *pmap;                  // Arch-Specific page tables
    
    bits32_t flags;                     // Spawn() flags
    
    struct TimeVal syscall_start_time;  // Time of start of syscall for VM watchdog purposes.
    int watchdog_state;
    

    struct VMMsg vm_msg;
    
    
    
    LIST_ENTRY (Process) alloc_link;
    LIST_ENTRY (Process) free_entry;
    
    int exit_status;                     // Exit() error code

    struct Rendez *sleeping_on;
    LIST_ENTRY (Process) rendez_link;
    
    struct Rendez waitfor_rendez;
    int waiting_for;
    LIST (Handle) pending_handle_list;
    LIST (Handle) close_handle_list;     // To be closed in KernelExit()
    
    int sighangup_handle;
    int sigterm_handle;

    int namespace_handle;
    
    char **argv;
    int argc;
    char **envv;
    int envc;
    
    int vm_retries;
};







/*
 * Message passing link between two handles
 * CONSIDER: Split into two Port structures pointing to each other.
 */
 
struct Channel
{
    LIST_ENTRY (Channel) link;     // Free list link
    int handle[2];                 // Handles for both channel endpoints
    LIST (Parcel) msg_list[2];     // Endpoint's list of pending messages
};



/*
 * Typedefs for linked lists
 */

LIST_TYPE (Channel) channel_list_t;
LIST_TYPE (Handle) handle_list_t;
LIST_TYPE (ISRHandler) isrhandler_list_t;
LIST_TYPE (Process) process_list_t;
CIRCLEQ_TYPE (Process) process_circleq_t;




/*
 * Function Prototypes
 */


// event.c

SYSCALL int CheckFor (int handle);
SYSCALL int WaitFor (int handle);
void DoRaiseEvent (int handle);
void DoClearEvent (struct Process *proc, int handle);


// handle.c

SYSCALL int CloseHandle (int handle);
void ClosePendingHandles (void);
void DoCloseSystemEvent (int h);
void *GetObject (struct Process *proc, int handle, int type);
void SetObject (struct Process *proc, int handle, int type, void *object);
struct Handle *FindHandle (struct Process *proc, int h);
int PeekHandle (int index);
int AllocHandle (void);
void FreeHandle (int handle);


// info.c
 
SYSCALL int SystemInfo (sysinfo_t *si);
SYSCALL int ProcessInfo (processinfo_t *pi);


// interrupt.c

SYSCALL int AddInterruptHandler (int irq);
int DoCloseInterruptHandler (int handle);
struct ISRHandler *AllocISRHandler(void);
void FreeISRHandler (struct ISRHandler *isr_handler);


// proc.c

SYSCALL int Spawn (struct SpawnArgs *user_sa, void **user_segments, int segment_cnt);
struct Process *AllocProcess (void);
void FreeProcess (struct Process *proc);
struct Process *GetProcess (int idx);


// join.c

SYSCALL int Join (int pid, int *status);
int DoCloseProcess (int handle);


// exit.c

SYSCALL void Exit (int status);
void DoExit (int status);


// sched.c

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




// proc/msg.c

SYSCALL int PutMsg (int port_h, void *msg, bits32_t flags);
SYSCALL ssize_t GetMsg (int handle, void **rcv_msg);
int CreateChannel (int result[2]);
int DoCloseChannel (int h);
SYSCALL int IsAChannel (int handle1, int handle2);

SYSCALL int PutHandle (int port_h, int h, bits32_t flags);
SYSCALL int GetHandle (int port_h);


// proc/notification.c


// Architecture-specific 
             

void ArchAllocProcess (struct Process *proc, void *entry, void *stack);
void ArchFreeProcess (struct Process *proc);             
void SwitchTasks (struct TaskState *current, struct TaskState *next, struct CPU *cpu);
int MaskInterrupt (int irq);
int UnmaskInterrupt (int irq);

void ContinueTo (void (*func) (void));


#endif


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
 *
 */
 
#define NSPAWNSEGMENTS      32
#define NSYSPORT            10


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
    void *segment[NSPAWNSEGMENTS];
    int segment_cnt;
    int handle[NSYSPORT];
    int handle_cnt;
};




/*
 * PutMsg() and PutHandle() flags
 */


#define MSG_GRANT_ONCE               (1<<0)
#define MSG_SILENT                   (1<<1)




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
#define PROC_STATE_SLEEP                3000




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
#define HANDLE_TYPE_NOTIFICATION        6

#define HANDLEF_GRANTED_ONCE            (1<<0)




/*
 * struct Handle;
 */

struct Handle
{
    int type;
    int pending;
    struct Process *owner;
    void *object;
    bits32_t flags;
    LIST_ENTRY (Handle) link;
    
    struct Parcel *parcel;
};





/*
 * struct Process;
 */

struct Process
{
    struct TaskState task_state;        // Arch-specific state

    int handle;                         // Handle ID for this process
    
    int sched_policy;                   
    int state;                          // Process state
    
    int priority;                       // real-time task priority
    int quanta_used;                    

    int tickets;                        // Stride scheduler state
    int stride;
    int remaining;
    int64 pass;
    
    bits32_t flags;                     // Spawn() flags
    
    struct Timer watchdog;              // Timeout for sleeping system calls
    size_t virtualalloc_sz;             // Size of memory requested by virtualalloc
    
    void (*continuation_function)(void);
    
    CIRCLEQ_ENTRY (Process) sched_entry;    // real-time run-queue
    LIST_ENTRY (Process) stride_entry;     

    
    struct Pmap pmap;                   // Arch-Specific page tables
    
    LIST_ENTRY (Process) free_entry;
    
    int exit_status;                    // Exit() error code

    struct Rendez *sleeping_on;
    LIST_ENTRY (Process) rendez_link;
    
    struct Rendez waitfor_rendez;
    int waiting_for;
    LIST (Handle) pending_handle_list;
    LIST (Handle) close_handle_list;
        
    int system_port[NSYSPORT];
};




/*
 * State notification shared between two handles
 */
 
struct Notification
{
    LIST_ENTRY (Notification) link;    // Free list link
    int handle[2];                  // Handles for both endpoints
    int state;                      // Single state variable shared between
                                    // both endpoints
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
LIST_TYPE (Notification) notification_list_t;
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
void *GetObject (struct Process *proc, int handle, int type);
void SetObject (struct Process *proc, int handle, int type, void *object);
struct Handle *FindHandle (struct Process *proc, int h);
int PeekHandle (int index);
int AllocHandle (void);
void FreeHandle (int handle);



// interrupt.c

SYSCALL int AddInterruptHandler (int irq);
int DoCloseInterruptHandler (int handle);
struct ISRHandler *AllocISRHandler(void);
void FreeISRHandler (struct ISRHandler *isr_handler);



// proc.c


SYSCALL int Spawn (struct SpawnArgs *sa);
SYSCALL int GetSystemPorts (int *sysports, int cnt);
int ValidateSystemPorts (int *sysports, int handle_cnt);
void TransferSystemPorts (struct Process *proc, int *sysports);
        
   
bool IsPrivileged (struct Process *proc);


struct Process *AllocProcess (void);
void FreeProcess (struct Process *proc);


// join.c


SYSCALL int Join (int pid, int *status);


// exit.c

SYSCALL void Exit (int status);
void DoExit (int status);


// kill.c

int DoCloseProcess (int handle);


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
void WakeupProcess (struct Process *proc);




// proc/msg.c

SYSCALL int PutMsg (int port_h, void *msg, bits32_t flags);
SYSCALL ssize_t GetMsg (int handle, void **rcv_msg, bits32_t access);
int CreateChannel (int result[2]);
int DoCloseChannel (int h);
SYSCALL int IsAChannel (int handle1, int handle2);

SYSCALL int PutHandle (int port_h, int h, bits32_t flags);
SYSCALL int GetHandle (int port_h, int *rcv_h);


// proc/notification.c

int PutNotification (int handle, int value);
int GetNotification (int handle);
int CreateNotification (int result[2]);
int DoCloseNotification (int handle);


// Architecture-specific 
             

void ArchAllocProcess (struct Process *proc, void *entry, void *stack);
void ArchFreeProcess (struct Process *proc);             
void SwitchTasks (struct TaskState *current, struct TaskState *next, struct CPU *cpu);
int MaskInterrupt (int irq);
int UnmaskInterrupt (int irq);




#endif


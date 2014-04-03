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
 * Number of clock ticks before process yields
 */

#define PROCESS_QUANTA                  2
#define SCHED_OTHER                     0
#define SCHED_RR                        1
#define SCHED_FIFO                      2
#define SCHED_IDLE                      -1



/*
 * Stride Scheduling constants
 */
 
#define STRIDE1                         1000000
#define STRIDE_DEFAULT_TICKETS          100
#define STRIDE_MAX_TICKETS              800




/*
 * Kernel condition variable for Sleep()/Wakeup() calls
 */
 
struct Rendez
{
    LIST (Process) process_list;
};






/*
 * struct ISRHandler
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
#define HANDLE_TYPE_CONDITION           6

#define HANDLEF_GRANT_ONCE               (1<<0)



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
};



/*
 * System port handles passed in an array to a child process in Spawn().
 * Can be retrieved by a process using GetSystemPorts.
 */

#define SYSTEM_PORT         0
#define EXCEPTION_PORT      1
#define ROOT_DIR_PORT       2
#define PROGRAM_DIR_PORT    3
#define CURRENT_DIR_PORT    4
#define STDIN_PORT          5
#define STDOUT_PORT         6
#define STDERR_PORT         7    
#define TERMINATE_PORT      8

#define NSYSPORT            9



/*
 * Spawn() flags
 */

#define PROCF_EXECUTIVE         (1<<0) 
#define PROCF_ALLOW_IO          (1<<1)


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
    
    struct Timer watchdog;
    
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
 *
 */
 
struct Notification
{
    LIST_ENTRY (Condition) link;    // Free list link
    int handle[2];                  // Handles for both endpoints
    int state;                      // Single state variable shared between
                                    // both endpoints
};




/*
 * struct Channel
 */
 
struct Channel
{
    LIST_ENTRY (Channel) link;      // Free list link
    int handle[2];                  // Handles for both channel endpoints
    LIST (MemArea) msg_list[2];     // Endpoint's list of pending messages
};




LIST_TYPE (Channel) channel_list_t;
LIST_TYPE (Notification) notification_list_t;
LIST_TYPE (Handle) handle_list_t;
LIST_TYPE (ISRHandler) isrhandler_list_t;
LIST_TYPE (Process) process_list_t;
CIRCLEQ_TYPE (Process) process_circleq_t;



// event.c

SYSCALL int CheckFor (int handle);
SYSCALL int WaitFor (int handle);
void DoRaiseEvent (int handle);
void DoClearEvent (struct Process *proc, int handle);


// handle.c

SYSCALL int GrantHandle (int r, int h, bits32_t flags);
SYSCALL int CloseHandle (int handle);
void ClosePendingHandles (void);
void *GetObject (struct Process *proc, int handle, int type);
void SetObject (struct Process *proc, int handle, int type, void *object);
struct Handle *GetHandle (struct Process *proc, int h);
int PeekHandle (int index);
int AllocHandle (void);
void FreeHandle (int handle);



// interrupt.c

SYSCALL int AddInterruptHandler (int irq);
int DoCloseInterruptHandler (int handle);
struct ISRHandler *AllocISRHandler(void);
void FreeISRHandler (struct ISRHandler *isr_handler);



// proc.c

SYSCALL int Spawn (vm_addr *segtable, int segcnt, void *entry, void *stack,
    int *sysports, bits32_t flags);

int ValidateSystemPorts (int *sysports);
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

SYSCALL int PutMsg (int handle, void **iov_table, int niov);
SYSCALL ssize_t GetMsg (int handle, void **msg, bits32_t access);
int CreateChannel (int result[2]);
int DoCloseChannel (int h);
SYSCALL int IsAChannel (int handle1, int handle2);
SYSCALL int GetSystemPorts (int *sysports);

SYSCALL int InjectHandle (vm_addr addr, int h, bits32_t flags);
SYSCALL int ExtractHandle (vm_addr addr);


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


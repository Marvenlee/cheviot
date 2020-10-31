#ifndef KERNEL_PROC_H
#define KERNEL_PROC_H

#include <kernel/arch.h>
#include <kernel/lists.h>
#include <kernel/timer.h>
#include <kernel/types.h>
#include <kernel/vm.h>
#include <sys/execargs.h>
#include <sys/interrupts.h>
#include <kernel/signal.h>

/*
 * Forward declarations
 */

struct Process;
struct VNode;
struct Filp;
struct Pid;
struct Msg;

LIST_TYPE(Process) process_list_t;
CIRCLEQ_TYPE(Process) process_circleq_t;
LIST_TYPE(Pid) pid_list_t;


/*
 * Kernel condition variable for Sleep()/Wakeup() calls
 */

struct Rendez {
  LIST(Process) blocked_list;
};


struct Mutex {
  int locked;
  struct Process *owner;
  LIST(Process) blocked_list;
};


struct VNode;



/*
 *
 */

typedef struct SysInfo {
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

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define EXIT_FATAL 2
#define EXIT_KILLED 3

/*
 * Process states
 */

#define PROC_STATE_UNALLOC 0
#define PROC_STATE_INIT 100
#define PROC_STATE_ZOMBIE 200
#define PROC_STATE_RUNNING 300
#define PROC_STATE_READY 500
#define PROC_STATE_SEND 600

//#define PROC_STATE_WAITPID              700

#define PROC_STATE_RENDEZ_BLOCKED 800
#define PROC_STATE_BKL_BLOCKED 1200

/*
 * Scheduling policy flags and kernel constants
 */

#define SCHED_OTHER 0
#define SCHED_RR 1
#define SCHED_FIFO 2
#define SCHED_IDLE -1

#define STRIDE1 1000000
#define STRIDE_DEFAULT_TICKETS 100
#define STRIDE_MAX_TICKETS 800
#define PROCESS_QUANTA 2


#define PROCF_USER 0          // User process
#define PROCF_KERNEL (1 << 0) // Kernel task: For kernel daemons
#define PROCF_ALLOW_IO (1 << 1)

/*
 * waitpid options
 */
#define WNOHANG 1
#define WUNTRACED 2

#define MAX_TIMER 8
#define MAX_INTERRUPT 8

#define NPROC_FD 32

/*
 * struct Process;
 */

struct Process {
  struct TaskCatch catch_state;
  struct CPU *cpu;
  struct ExceptionState exception_state;
  void *context; // Reschedule context
  struct Process *parent;

  struct Rendez *sleeping_on;
  LIST_ENTRY(Process) blocked_link;
  struct Mutex *rendez_mutex;
  
  bool eintr;

  struct Rendez rendez;
  struct Timer sleep_timer;
  struct Timer timeout_timer;
  bool timeout_expired;
  struct Timer alarm;
  
  CIRCLEQ_ENTRY(Process) sched_entry; // real-time run-queue
  LIST_ENTRY(Process) stride_entry;

  int state; // Process state
  bits32_t flags;
  
  int log_level;

  struct Signal signal;

  int sched_policy;
  int quanta_used;
  int rr_priority;    // real-time task priority
  int stride_tickets; // Stride scheduler state
  int stride;
  int stride_remaining;
  int64_t stride_pass;

  struct AddressSpace as;

  int exit_status; // Exit() error code
  LIST(Process) child_list;
  LIST_ENTRY(Process) child_link;

  int pid;
  int uid;
  int gid;
  int euid;
  int egid;
  int pgrp;
//  int sid;
  bool in_use; 

  mode_t default_mode;
  struct VNode *current_dir;
  struct VNode *root_dir;

  struct Msg *msg;

  short poll_pending;
  short poll_interested_in;

  // TODO: Move these out of process, make array of uint16_t.
  // Perhaps 2 timers per process, separate structs, alarm and freerunning.
  // Interrupts could be small array
  // Or single interrupt handler, passed the interrupt in question.

  bool inkernel;
  
//  int max_fd;
  struct Filp *fd_table[NPROC_FD];
  uint32_t close_on_exec[NPROC_FD/32];
};



/*
 * Function Prototypes
 */

// interrupt.c


int CreateInterrupt(int irq, void (*callback)(int irq, struct InterruptAPI *api));


// TODO: Move Exec protos into filesystem.c
int Exec(char *filename, struct execargs *args);
int KExecImage(void *image, struct execargs *_args);
int Kexec(char *filename);

void Exit(int status);
int WaitPid(int handle, int *status, int options);

// proc.c

int Fork(void);

struct Process *AllocProcess(void);
void FreeProcess(struct Process *proc);
struct Process *GetProcess(int idx);

struct Process *GetProcess(int pid);
int GetProcessPid(struct Process *proc);
int GetPid(void);


// sched.c

void SchedLock(void);
void SchedUnlock(void);

void Reschedule(void);
void SchedReady(struct Process *proc);
void SchedUnready(struct Process *proc);
SYSCALL int ChangePriority(int priority);
//void RaiseSignal(struct Process *proc, uint32_t signals);

void InitRendez(struct Rendez *rendez);
void TaskSleep(struct Rendez *rendez);
void TaskWakeup(struct Rendez *rendez);
void TaskWakeupAll(struct Rendez *rendez);
void TaskWakeupFromISR(struct Rendez *rendez);

// void WakeupProcess (struct Process *proc);
void InitMutex(struct Mutex *mutex);
void MutexLock(struct Mutex *mutex);
void MutexUnlock(struct Mutex *mutex);

void KernelLock(void);
void KernelUnlock(void);
bool IsKernelLocked(void);

// Architecture-specific

void GetContext(uint32_t *context);
int SetContext(uint32_t *context);

int ArchForkProcess(struct Process *proc, struct Process *current);
void ArchInitExec(struct Process *proc, void *entry_point,
                  void *stack_pointer, struct execargs *args);
void ArchFreeProcess(struct Process *proc);
// void SwitchTasks (struct  *current, struct TaskState *next, struct CPU *cpu);

int PollNotifyFromISR(struct InterruptAPI *api, uint32_t mask, uint32_t events);
int UnmaskInterrupt(int irq);
int MaskInterrupt(int irq);


void InterruptLock(void); // Effectively spinlock_irq_save(&intr_spinlock);
void InterruptUnlock(void);

struct Process *CreateProcess(void (*entry)(void), int policy, int priority, bits32_t flags, struct CPU *cpu);

#endif

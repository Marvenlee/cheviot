#ifndef KERNEL_PROC_H
#define KERNEL_PROC_H

#include <kernel/arch.h>
#include <kernel/lists.h>
#include <kernel/msg.h>
#include <kernel/timer.h>
#include <kernel/types.h>
#include <kernel/vm.h>
#include <sys/execargs.h>
#include <sys/interrupts.h>
#include <kernel/signal.h>
#include <kernel/sync.h>
#include <kernel/filesystem.h>
#include <time.h>


// Forward declarations
struct Process;
struct VNode;
struct Filp;
struct Pid;

// Process related list types
LIST_TYPE(Process, process_list_t, process_link_t);
CIRCLEQ_TYPE(Process, process_circleq_t, process_cqlink_t);
LIST_TYPE(Pid, pid_list_t, pid_link_t);


/* @brief   Structure containing system configuration
 * TODO: Probably move this to newlib
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
} sysinfo_t;


// Process.state
#define PROC_STATE_UNALLOC 0
#define PROC_STATE_INIT 100
#define PROC_STATE_ZOMBIE 200
#define PROC_STATE_RUNNING 300
#define PROC_STATE_READY 500
#define PROC_STATE_SEND 600
#define PROC_STATE_RENDEZ_BLOCKED 800
#define PROC_STATE_BKL_BLOCKED 1200

// Scheduler related
#define SCHED_IDLE -1             // Scheduling policy not defined in C library
#define SCHED_QUANTA_JIFFIES  5   // Reschedule if task has ran for this many timer ticks

// Process.flags
#define PROCF_USER     0          // User process
#define PROCF_KERNEL   (1 << 0)   // Kernel task: For kernel daemons
#define PROCF_ALLOW_IO (1 << 1)

// Max number of processes and per-process resources
#define NPROCESS        256
#define NR_ISR_HANDLER  64
#define MAX_TIMER       8


/* @brief   Process state
 */
struct Process {
  struct TaskCatch catch_state;
  struct CPU *cpu;
  struct ExceptionState exception_state;
  void *context;

  int state;              // Process state

  process_cqlink_t sched_entry; // real-time run-queue
  int sched_policy;
  int quanta_used;        // number of ticks the process has run without blocking
  int priority;           // effective priority of process
  int desired_priority;   // default priority of process
    
  struct Process *parent;

  struct Rendez *sleeping_on;
  process_link_t blocked_link;

  struct AddressSpace as;
  
  struct Rendez rendez;
  struct Timer sleep_timer;
  struct Timer timeout_timer;
  bool timeout_expired;
  struct Timer alarm;
  struct MsgPort reply_port;

  bits32_t flags;
  bool in_use;            // Process slot is in use, remove when dynamically allocating process
  bool eintr;
  
  int log_level;
  
  struct Signal signal;

  int exit_status;        // Exit() error code
  process_list_t child_list;
  process_link_t child_link;
  
  // Move into struct Identity    
  int pid;
  int uid;
  int gid;
  int euid;
  int egid;
  int pgrp;
  int sid;
  
  struct FProcess *fproc;  
};


/*
 * Function Prototypes
 */

// proc/proc.c
int sys_fork(void);
void sys_exit(int status);
int sys_waitpid(int handle, int *status, int options);

struct Process *AllocProcess(void);
void FreeProcess(struct Process *proc);
struct Process *GetProcess(int pid);
int GetProcessPid(struct Process *proc);
int GetPid(void);

// proc/sched.c
void SchedLock(void);
void SchedUnlock(void);

void Reschedule(void);
void SchedReady(struct Process *proc);
void SchedUnready(struct Process *proc);

void InitRendez(struct Rendez *rendez);
void TaskSleep(struct Rendez *rendez);
int TaskTimedSleep(struct Rendez *rendez, struct timespec *ts);
void TaskWakeup(struct Rendez *rendez);
void TaskWakeupAll(struct Rendez *rendez);
void TaskWakeupFromISR(struct Rendez *rendez);

void KernelLock(void);
void KernelUnlock(void);
bool IsKernelLocked(void);


// Architecture-specific

void GetContext(uint32_t *context);
int SetContext(uint32_t *context);

int arch_fork_process(struct Process *proc, struct Process *current);
void arch_init_exec(struct Process *proc, void *entry_point,
                  void *stack_pointer, struct execargs *args);
void arch_free_process(struct Process *proc);
struct Process *create_process(void (*entry)(void), int policy, int priority, bits32_t flags, struct CPU *cpu);

int arch_clock_gettime(int clock_id, struct timespec *ts);

// Static assertions
// STATIC_ASSERT(sizeof(struct Process) < PAGE_SIZE - KERNEL_STACK_MIN_SZ, "struct Process to large");

#endif

#ifndef KERNEL_SIGNAL_H
#define KERNEL_SIGNAL_H

#include <kernel/lists.h>
#include <kernel/types.h>
#include <signal.h>
//#include <ucontext.h>

// Forward Declarations
struct Process;


/*
 * Bitmap masks of some signal properties
 */

#define SIGBIT(sig)	(1UL<<(sig-1)) 


#define SIGCANTMASK (SIGBIT(SIGKILL) | SIGBIT(SIGSTOP))
#define STOPSIGMASK (SIGBIT(SIGSTOP) | SIGBIT(SIGTSTP) | SIGBIT(SIGTTIN) | SIGBIT(SIGTTOU))
#define CONTSIGMASK (SIGBIT(SIGFCONT))
#define SYNCSIGMASK (SIGBIT(SIGILL) | SIGBIT(SIGTRAP) | SIGBIT(SIGBUS) | SIGBIT(SIGFPE) | SIGBIT(SIGSEGV))




/*
 * Flags used in the sigprop[] array
 */
 
#define SP_KILL     (1<<0)
#define SP_CORE     (1<<1)
#define SP_NORESET  (1<<2)
#define SP_CANTMASK (1<<3)
#define SP_CONT     (1<<4)
#define SP_STOP     (1<<5)
#define SP_TTYSTOP  (1<<6)


/*
 * User/nix signal state of a Process.
 */

struct Signal {
  _sig_func_ptr handler[NSIG - 1]; /* Action for each signal, func, SIG_DFL, SIG_IGN */
  sigset_t handler_mask[NSIG - 1]; /* Mask to add to current sig_mask during handler.*/

  int8_t si_code[NSIG - 1];             /* code (user, message etc) */ 
  vm_addr sigsegv_ptr;
  vm_addr sigill_ptr;
  
  sigset_t sig_info; /* bitmap of signals that want SA_SIGINFO args. */

  sigset_t sig_mask;    /* Current signal mask */
  sigset_t sig_pending; /* bitmap of signals awaiting delivery */

  sigset_t sig_resethand; /* Unreliable signals */
  sigset_t sig_nodefer;   /* Don't automatically mask delivered signal */

  sigset_t sigsuspend_oldmask;
  bool use_sigsuspend_mask;

  void (*trampoline)(void);
};

/*
 * Variables
 */

extern const uint32_t sigprop[NSIG];

/*
 * Prototypes
 */


int sys_kill(int pid, int signal);
int SigAction(int how, const struct sigaction *act, struct sigaction *oact);
int SigSuspend(const sigset_t *mask);
int SigProcMask(int how, const sigset_t *set, sigset_t *oset);
int SigPending(sigset_t *set);
void SigSetTrampoline(void (*func)(void));
void SigExit(int signal);
void SigInit(struct Process *dst);
void SigFork(struct Process *src, struct Process *dst);
void SigExec(struct Process *dst);
void DoSignalDefault(int sig);


#endif

#ifndef KERNEL_KQUEUE_H
#define KERNEL_KQUEUE_H


#include <kernel/lists.h>
#include <kernel/types.h>
#include <sys/event.h>

// Forward declarations
struct InterruptAPI;
struct KQueue;
struct KNote;
struct Process;


// List types
LIST_TYPE(KNote, knote_list_t, knote_link_t);
LIST_TYPE(KQueue, kqueue_list_t, kqueue_link_t);

/*
 * Constants
 */
#define NR_KQUEUE 128
#define NR_KNOTE  2048
#define KNOTE_HASH_SZ 64

/*
 * KQueue
 */
struct KQueue
{
  bool busy;
  int reference_cnt;  
  //  struct Process *owner;
  struct Rendez busy_rendez;      // Are two rendez needed?
  struct Rendez event_rendez;  
  kqueue_link_t free_link;    
  knote_list_t knote_list;
  knote_list_t pending_list;
};


/*
 *
 */
struct KNote
{
  knote_link_t link;          // free list
  knote_link_t hash_link;     // hash table lookup;  
  knote_link_t kqueue_link;   // kqueue's list of knotes
  knote_link_t pending_link;  // kqueue's list of pending knote events
  knote_link_t object_link;   // List of knotes attached to object being monitored (e.g. vnode, process)
  
  struct KQueue *kqueue;	    // which kqueue we belong to 
  bool enabled;               // knote is listening for events and can put on pending list
  bool pending;               // knote has an event pending
  bool on_pending_list;       // knote is already on the pending list.
  int hint;
	
  int ident;
  int filter;  
  int flags;
  int fflags;
  void *data;                 // kernel object it points to?
  void *udata;	              // opaque user data identifier
	
  void *object;               // opaque pointer to vnode, msgport, isr_handler, process  
};


/*
 * Prototypes
 */
 
void enable_knote(struct KQueue *kqueue, struct KNote *knote);
void disable_knote(struct KQueue *kqueue, struct KNote *knote);

int sys_kqueue(void);
int sys_kevent(int kq, const struct kevent	*changelist, int nchanges,
               struct	kevent *eventlist, int nevents, const struct timespec *timeout);

int knote(knote_list_t *note_list, int hint);

int close_kqueue(struct Process *proc, int fd);

struct KQueue *get_kqueue(struct Process *proc, int fd);
int alloc_fd_kqueue(struct Process *proc);
int free_fd_kqueue(struct Process *proc, int fd);
struct KQueue *alloc_kqueue(void);
void free_kqueue(struct KQueue *kqueue);

struct KNote *get_knote(struct KQueue *kq, struct kevent *ev);
struct KNote *alloc_knote(struct KQueue *kq, struct kevent *ev);
void free_knote(struct KQueue *kqueue, struct KNote *knote);
int knote_calc_hash(struct KQueue *kq, int ident, int filter);



#endif


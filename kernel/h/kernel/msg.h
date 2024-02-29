#ifndef MSG_H
#define MSG_H

#include <kernel/error.h>
#include <kernel/lists.h>
#include <kernel/types.h>
#include <kernel/sync.h>
#include <kernel/kqueue.h>
#include <sys/syscalls.h>
#include <unistd.h>



// Forward declarations
struct Msg;
struct Process;
struct MsgPort;
struct MsgID2Msg;

// List types
LIST_TYPE(Msg, msg_list_t, msg_link_t);

// Constants
#define MSGID_HASH_SZ       128



/* @brief   Kernel Message
 */
struct Msg
{
  msg_link_t link;
  msgid_t msgid;
  struct MsgPort *port;       // The port it is attached to or NULL if not attached.
                              // Set to reply_port on reply, so any msgid_to_msg fails after replymsg
                              
  struct MsgPort *reply_port; // The reply port to reply to
  int reply_status;  
  int siov_cnt;
  struct IOV *siov;
  int riov_cnt;
  struct IOV *riov;
};


/* @brief   Message Port for interprocess communication
 */
struct MsgPort
{
  struct Rendez rendez;
  msg_list_t pending_msg_list;
  knote_list_t knote_list;    
  void *context;              // For pointer to superblock or other data
};



// Macros
#define NELEM(a) (sizeof(a) / sizeof(*a))   // Calculate number of elements in an array

/*
 * Prototypes
 */
int sys_sendrec(int fd, int siov_cnt, struct IOV *siov, int riov_cnt, struct IOV *riov);
int sys_getmsg(int server_fd, msgid_t *msgid, void *buf, size_t buf_sz);
int sys_replymsg(int server_fd, msgid_t *msgid, int status, void *buf, size_t buf_sz);
int sys_readmsg(int server_fd, msgid_t *msgid, void *buf, size_t buf_sz, off_t offset);
int sys_writemsg(int server_fd, msgid_t *msgid, void *buf, size_t buf_sz, off_t offset);

int ksendmsg(struct MsgPort *msgport, int siov_cnt, struct IOV *siov, int riov_cnt, struct IOV *riov);
int kputmsg(struct MsgPort *msgport, struct Msg *msg);
int kreplymsg(struct Msg *msg);
struct Msg *kgetmsg(struct MsgPort *port);
struct Msg *kpeekmsg(struct MsgPort *port);

int kwaitport(struct MsgPort *msgport, struct timespec *timeout);
int seekiov(int iov_cnt, struct IOV *iov, off_t offset, int *i, size_t *iov_remaining);

struct Msg *msgid_to_msg(struct MsgPort *port, msgid_t msgid);
msgid_t new_msgid(void);
void hash_msg(struct Msg *msg);
void unhash_msg(struct Msg *msg);

int init_msgport(struct MsgPort *msgport);
int fini_msgport(struct MsgPort *msgport);


#endif


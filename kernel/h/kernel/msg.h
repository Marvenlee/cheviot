#ifndef MSG_H
#define MSG_H

#include <kernel/error.h>
#include <kernel/lists.h>
#include <kernel/types.h>
#include <kernel/proc.h>

struct VNode;
struct MsgPort;
struct Buf;

struct IOV {
  void *addr;
  size_t size;
};

struct Msg {
  LIST_ENTRY(Msg) link;
  struct MsgPort *port;
  int pid;
  int type;
  int state;
  int iov_cnt;
  ssize_t offset;
  struct IOV *iov;
  struct Buf *buf;
  struct Rendez rendez;
  int reply_status;
};


struct MsgPort {
  struct VNode *server_vnode;

  LIST(Msg) pending_msg_list;   // pending list of vnodes
};


// Is message a synchronous SendMsg or async PutMsg.
#define MSG_SENDREC   1
#define MSG_ASYNC     2

#define MSG_STATE_SEND 1
#define MSG_STATE_RECEIVED 2
#define MSG_STATE_REPLIED 3


int InitMsgPort(struct MsgPort *msgport, struct VNode *vnode);
int KSendMsg(struct MsgPort *port, struct VNode *vnode, struct Msg *msg);
int KPutMsg(struct MsgPort *port, struct VNode *vnode, struct Msg *msg);
int ReceiveMsg(int server_fd, int *pid, void *buf, size_t buf_sz);
int ReplyMsg(int server_fd, int pid, int status);
int ReadMsg(int server_fd, int pid, void *buf, size_t buf_sz);
int WriteMsg(int server_fd, int pid, void *buf, size_t buf_sz);

#endif

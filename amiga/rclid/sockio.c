#include <exec/exec.h>

#include <proto/exec.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>

#include <sys/ioctl.h>
#include <netinet/tcp.h>

#ifdef DEBUG_SOCKIO
#define DEBUG
#endif

#include "log.h"
#include "sockio.h"
#include "listutil.h"
#include "timer.h"

#define SOCKIO_MSG_FREE     0xff

struct sockio_handle {
  int               socket;
  struct MsgPort   *user_port;
  struct MsgPort   *msg_port;
  struct List       free_list;
  struct List       recv_list;
  struct List       send_list;
  struct List       wait_list;
  sockio_msg_t     *msgs;
  timer_handle_t   *timer;
  ULONG             max_msgs;
  ULONG             rx_actual;
  ULONG             tx_actual;
  ULONG             msg_mask;
  ULONG             timer_mask;
  ULONG             state;
  UBYTE             *push_back_buf;
  ULONG             push_back_size;
};

sockio_handle_t *sockio_init(int socket, struct MsgPort *user_port, ULONG max_msgs)
{
  sockio_handle_t *sio = (sockio_handle_t *)AllocVec(sizeof(sockio_handle_t), MEMF_ANY | MEMF_CLEAR);
  if(sio == NULL) {
    return NULL;
  }

  sio->socket = socket;
  sio->user_port = user_port;
  sio->max_msgs = max_msgs;
  sio->state = SOCKIO_STATE_OK;

  NewList(&sio->free_list);
  NewList(&sio->recv_list);
  NewList(&sio->send_list);
  NewList(&sio->wait_list);

  // make socket non blocking
  ULONG yes=TRUE;
  IoctlSocket(socket, FIONBIO, (char *)&yes);

  // no delay
  setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(ULONG));

  // msg port
  sio->msg_port = CreateMsgPort();
  if(sio->msg_port == NULL) {
    goto fail;
  }
  sio->msg_mask = 1 << sio->msg_port->mp_SigBit;

  // alloc msgs
  ULONG msg_size = sizeof(sockio_msg_t) + 3 & ~3;
  ULONG size = msg_size * max_msgs;
  sio->msgs = AllocVec(size, MEMF_CLEAR | MEMF_ANY);
  if(sio->msgs == NULL) {
    goto fail;
  }

  // build free list of messages
  for(ULONG i=0;i<max_msgs;i++) {
    AddTail(&sio->free_list, (struct Node *)&sio->msgs[i]);
    sio->msgs[i].type = SOCKIO_MSG_FREE;
  }

  // setup timer
  sio->timer = timer_init();
  if(sio->timer == NULL) {
    goto fail;
  }
  sio->timer_mask = timer_get_sig_mask(sio->timer);

  // all ok
  return sio;

fail:
  sockio_exit(sio);
  return NULL;
}

void sockio_flush(sockio_handle_t *sio)
{
  struct Node *node;

  // clean up recv list
  node = GetHead(&sio->recv_list);
  while(node != NULL) {
    sockio_msg_t *msg = (sockio_msg_t *)node;
    struct Node *next_node = GetSucc(node);
    LOG(("sockio: flush recv msg %lx\n", msg));

    // set EOF (size == 0)
    msg->buffer.size = 0;

    // remove and reply job
    Remove(node);
    ReplyMsg((struct Message *)msg);

    node = next_node;
  }

  // clean up send list
  node = GetHead(&sio->send_list);
  while(node != NULL) {
    sockio_msg_t *msg = (sockio_msg_t *)node;
    struct Node *next_node = GetSucc(node);
    LOG(("sockio: flush send msg %lx\n", msg));

    // remove and reply job
    Remove(node);
    ReplyMsg((struct Message *)msg);

    node = next_node;
  }

  // clean up wait list
  node = GetHead(&sio->wait_list);
  while(node != NULL) {
    sockio_msg_t *msg = (sockio_msg_t *)node;
    timer_job_t *job = (timer_job_t *)msg->buffer.data;
    LOG(("sockio: flush wait msg %lx -> job %lx\n", msg, job));
    timer_stop(job);

    struct Node *next_node = GetSucc(node);

    // clear wait flag
    msg->buffer.size = 0;

    // remove and reply job
    Remove(node);
    ReplyMsg((struct Message *)msg);

    node = next_node;
  }
}

void sockio_exit(sockio_handle_t *sio)
{
  if(sio == NULL) {
    return;
  }

  sockio_flush(sio);

  if(sio->timer != NULL) {
    timer_exit(sio->timer);
  }

  if(sio->msgs != NULL) {
    FreeVec(sio->msgs);
  }

  if(sio->msg_port != NULL) {
    DeleteMsgPort(sio->msg_port);
  }

  FreeVec(sio);
}

static sockio_msg_t *alloc_msg(sockio_handle_t *sio, UBYTE type, APTR data, ULONG size, ULONG min_size)
{
  sockio_msg_t *msg = (sockio_msg_t *)RemHead(&sio->free_list);
  if(msg == NULL) {
    return NULL;
  }

  msg->msg.mn_ReplyPort = sio->user_port;
  msg->type = type;
  msg->buffer.data = data;
  msg->buffer.size = size;
  msg->min_size = min_size;
  msg->user_data = NULL;

  return msg;
}

void sockio_free_msg(sockio_handle_t *sio, sockio_msg_t *msg)
{
  // return to free list
  LOG(("sockio: free msg: %lx\n", msg));
  AddTail(&sio->free_list, (struct Node *)msg);
  msg->type = SOCKIO_MSG_FREE;
}

static BOOL reply_end_msg(sockio_handle_t *sio)
{
  sockio_msg_t *msg = alloc_msg(sio, SOCKIO_MSG_END, NULL, 0, 0);
  if(msg == NULL) {
    LOG(("sockio: no mem for end msg!\n"));
    return FALSE;
  }

  LOG(("scokio: reply end msg!\n"));
  ReplyMsg((struct Message *)msg);
  return TRUE;
}

static BOOL reply_error_msg(sockio_handle_t *sio, ULONG error)
{
  // put error in min_size
  sockio_msg_t *msg = alloc_msg(sio, SOCKIO_MSG_ERROR, NULL, 0, error);
  if(msg == NULL) {
    LOG(("sockio: no mem for error msg!\n"));
    return FALSE;
  }

  LOG(("scokio: reply error msg!\n"));
  ReplyMsg((struct Message *)msg);
  return TRUE;
}

void sockio_end(sockio_handle_t *sio)
{
  if(sio->state == SOCKIO_STATE_OK) {
    sio->state = SOCKIO_STATE_EOF;
    sockio_flush(sio);
    reply_end_msg(sio);
  }
}

static void handle_msg(sockio_handle_t *sio, sockio_msg_t *msg)
{
  LOG(("sockio: incoming msg: %lx: type=%ld\n", msg, (ULONG)msg->type));
  switch(msg->type) {
  case SOCKIO_MSG_RECV:
    LOG(("sockio: add recv %lx\n", msg));
    AddTail(&sio->recv_list, (struct Node *)msg);
    break;
  case SOCKIO_MSG_SEND:
    LOG(("sockio: add send %lx\n", msg));
    AddTail(&sio->send_list, (struct Node *)msg);
    break;
  case SOCKIO_MSG_WAIT_CHAR:
    LOG(("sockio: add wait char %lx\n", msg));
    AddTail(&sio->wait_list, (struct Node *)msg);
    break;
  }
}

static void handle_timer(sockio_handle_t *sio)
{
  timer_job_t *job;

  while((job = timer_get_next_done_job(sio->timer)) != NULL) {
    LOG(("sockio: TIMER STOP job %lx -> done!", job));
    timer_stop(job);
    // get msg
    sockio_msg_t *msg = (sockio_msg_t *)timer_job_get_user_data(job);
    LOG(("sockio: -> wait msg %lx\n", msg));
    // clear wait flag
    msg->buffer.size = 0;
    // remove from wait list
    Remove((struct Node *)msg);
    // reply to user
    ReplyMsg((struct Message *)msg);
  }
}

static int handle_push_back(sockio_handle_t *sio, sockio_msg_t *rx_msg)
{
  int done = 0;
  ULONG push_size = sio->push_back_size;
  APTR  push_buf = sio->push_back_buf;

  buf_t *rx_buf = &rx_msg->buffer;
  ULONG rx_size = rx_buf->size - sio->rx_actual;
  UBYTE *ptr = rx_buf->data + sio->rx_actual;

  // more in push buf than in buf size
  if(push_size > rx_size) {
    // adjust push buf
    sio->push_back_size -= rx_size;
    sio->push_back_buf += rx_size;
    push_size = rx_size;
  } else {
    sio->push_back_size = 0;
    sio->push_back_buf = NULL;
  }

  LOG(("sockio: push back RX: %lx size=%ld\n", rx_msg, push_size));
  CopyMem(push_buf, ptr, push_size);
  sio->rx_actual += push_size;

  // is rx done?
  if(sio->rx_actual >= rx_msg->min_size) {
    // store actual size
    rx_buf->size = sio->rx_actual;
    LOG(("sockio: push back RX reply: %lx size=%ld\n", rx_msg, rx_buf->size));
    // remove from recv list
    RemHead(&sio->recv_list);
    // clear actual
    sio->rx_actual = 0;
    // reply message
    ReplyMsg((struct Message *)rx_msg);

    done = 1;
  }

  return done;
}

/* wait for socket io or other signals and return SOCKIO_HANDLE flags */
void sockio_wait_handle(sockio_handle_t *sio, ULONG *sig_mask)
{
  fd_set rx_fds;
  fd_set tx_fds;
  sockio_msg_t *rx_msg = NULL;
  sockio_msg_t *tx_msg = NULL;
  sockio_msg_t *wait_msg = NULL;
  buf_t *rx_buf = NULL;
  buf_t *tx_buf = NULL;

  // if set to EOF then flush all pending requests and
  // do not wait on actual transfers
  if(sio->state != SOCKIO_STATE_OK) {
    LOG(("sockio: WAIT flush on EOF\n"));
    sockio_flush(sio);
  }

  // pick rx buffer (if any)
  struct Node *rx_node = GetHead(&sio->recv_list);
  if(rx_node != NULL) {
    rx_msg = (sockio_msg_t *)rx_node;
    rx_buf = &rx_msg->buffer;
  }

  // pick tx buffer (if any)
  struct Node *tx_node = GetHead(&sio->send_list);
  if(tx_node != NULL) {
    tx_msg = (sockio_msg_t *)tx_node;
    tx_buf = &tx_msg->buffer;
  }

  // do we need to submit pushed back data?
  if((rx_buf != NULL) && (sio->push_back_size > 0)) {
    if(handle_push_back(sio, rx_msg)) {
      rx_msg = NULL;
      rx_buf = NULL;
    }
  }

  LOG(("sockio: WAIT rx_msg %lx tx_msg %lx\n", rx_msg, tx_msg));

  // prepare select

  // do we need to receive something?
  BOOL rx_pending = FALSE;
  FD_ZERO(&rx_fds);
  if((rx_buf != NULL) && (sio->rx_actual < rx_msg->min_size)) {
    // wait for possible receiption
    FD_SET(sio->socket, &rx_fds);
    rx_pending = TRUE;
  }

  // is something available for transmit?
  BOOL tx_pending = FALSE;
  FD_ZERO(&tx_fds);
  if((tx_buf != NULL) && (sio->tx_actual < tx_buf->size)) {
    FD_SET(sio->socket, &tx_fds);
    tx_pending = TRUE;
  }

  // wait char handling: wait for read if wait list is not empty
  BOOL wait_pending = FALSE;
  struct Node *wait_node = GetHead(&sio->wait_list);
  if(wait_node != NULL) {
    wait_msg = (sockio_msg_t *)wait_node;
    FD_SET(sio->socket, &rx_fds);
    wait_pending = TRUE;
  }

  // pre-fill sigmask with our port bits
  ULONG got_mask = sio->msg_mask | sio->timer_mask;
  if(sig_mask != NULL) {
    got_mask |= *sig_mask;
  }

  // do select and wait
  LOG(("sockio: ENTER WAIT: mask=%lx rx_pend=%ld tx_pend=%ld wait_pend=%ld\n",
    got_mask, (LONG)rx_pending, (LONG)tx_pending, (LONG)wait_pending));
  long wait_result = WaitSelect(sio->socket + 1, &rx_fds, &tx_fds, NULL, NULL, &got_mask);
  LOG(("sockio: LEAVE WAIT: res=%ld mask=%lx\n", wait_result, got_mask));

  // we got an error
  if(wait_result == -1) {
    sio->state = SOCKIO_STATE_ERROR;
    sockio_flush(sio);
    reply_error_msg(sio, SOCKIO_ERROR_WAIT);
    return;
  }

  // --- handle sig mask ---

  // handle own msg port
  if(got_mask & sio->msg_mask) {
    // own msg
    struct Message *msg;
    while((msg = GetMsg(sio->msg_port)) != NULL) {
      handle_msg(sio, (sockio_msg_t *)msg);
    }
  }

  // handle timer
  if(got_mask & sio->timer_mask) {
    handle_timer(sio);
  }

  // update mask (without our bits)
  if(sig_mask != NULL) {
    *sig_mask = got_mask & ~(sio->msg_mask | sio->timer_mask);
  }

  // end now. since only signals occurred
  if(wait_result == 0) {
    LOG(("sockio: wait RETURN ONLY SIGMASK %lx\n", got_mask));
    return;
  }

  // handle wait char
  if(wait_msg != NULL) {
    if(FD_ISSET(sio->socket, &rx_fds)) {
      LOG(("sockio: WAIT CHAR: got it! -> msg %lx\n", wait_msg));
      // set flag
      wait_msg->buffer.size = 1;
      // remove from wait list
      Remove((struct Node *)wait_msg);
      // get and stop job
      timer_job_t *job = (timer_job_t *)wait_msg->buffer.data;
      LOG(("sockio: TIMER stop job %lx\n", job));
      timer_stop(job);
      // reply to user
      ReplyMsg((struct Message *)wait_msg);
    }
  }

  BOOL end = FALSE;
  BOOL error = FALSE;

  // handle socket RX
  if(rx_buf != NULL) {
    if(FD_ISSET(sio->socket, &rx_fds) && rx_pending) {
      // try to read in missing bytes
      ULONG rx_size = rx_buf->size - sio->rx_actual;
      UBYTE *ptr = rx_buf->data + sio->rx_actual;
      LOG(("sockio: RX READY! recv up to %ld bytes (actual %ld)\n", rx_size, sio->rx_actual));
      long res = recv(sio->socket, ptr, rx_size, 0);
      LOG(("sockio: RX RESULT: %ld\n", res));
      if(res == -1) {
        // on EAGAIN simply repeat in next handle call
        if(Errno() != EAGAIN) {
          LOG(("sockio: RX ERROR: errno=%ld\n", Errno()));
          sio->state = SOCKIO_STATE_ERROR;
          error = TRUE;
        }
      }
      else if(res > 0) {
        // adjust buffer
        sio->rx_actual += res;
        LOG(("sockio: RX DATA: new size=%ld\n", sio->rx_actual));
      }
      else {
        LOG(("sockio: RX EOF!\n"));
        sio->state = SOCKIO_STATE_EOF;
        end = TRUE;
      }
    }

    // is rx done?
    if(sio->rx_actual >= rx_msg->min_size) {
      // store actual size
      rx_buf->size = sio->rx_actual;
      LOG(("sockio: RX reply: %lx size=%ld\n", rx_msg, rx_buf->size));
      // remove from recv list
      RemHead(&sio->recv_list);
      // clear actual
      sio->rx_actual = 0;
      // reply message
      ReplyMsg((struct Message *)rx_msg);
    }
  }

  // handle socket TX
  if(tx_buf != NULL) {
    if(FD_ISSET(sio->socket, &tx_fds) && tx_pending) {
      UBYTE *ptr = tx_buf->data + sio->tx_actual;
      ULONG tx_size = tx_buf->size - sio->tx_actual;
      LOG(("sockio: TX READY! send %ld bytes (actual %ld)\n", tx_size, sio->tx_actual));
      long res = send(sio->socket, ptr, tx_size, 0);
      LOG(("sockio: TX RESULT: %ld\n", res));
      if(res == -1) {
        // on EAGAIN try in next handle call
        if(Errno() != EAGAIN) {
          LOG(("sockio: TX ERROR: errno=%ld\n", Errno()));
          sio->state = SOCKIO_STATE_ERROR;
          error = TRUE;
        }
      }
      else if(res > 0) {
        sio->tx_actual += res;
        LOG(("sockio: TX_DATA: actual=%ld size=%ld\n", sio->tx_actual, tx_buf->size));
      }
      else {
        LOG(("sockio: TX EOF!\n"));
        sio->state = SOCKIO_STATE_EOF;
        end = TRUE;
      }
    }

    // is tx done?
    if(tx_buf->size == sio->tx_actual) {
      LOG(("sockio: TX reply: %lx size=%ld", tx_msg, tx_buf->size));
      // remove from send list
      RemHead(&sio->send_list);
      // clear actual
      sio->tx_actual = 0;
      // reply msg
      ReplyMsg((struct Message *)tx_msg);
    }
  }

  // sio end state reached?
  if(end) {
    LOG(("sockio: WAIT end reached %ld\n", sio->state));
    sockio_flush(sio);
    reply_end_msg(sio);
  }

  if(error) {
    LOG(("sockio: WAIT error reached %ld\n", sio->state));
    sockio_flush(sio);
    reply_error_msg(sio, SOCKIO_ERROR_IO);
  }

  LOG(("sockio: WAIT done. state=%ld\n", sio->state));
}

/* push some data back to receive buffer */
void sockio_push_back(sockio_handle_t *sio, APTR buf, ULONG size)
{
  LOG(("sockio: push back %ld bytes\n", size));
  sio->push_back_buf = buf;
  sio->push_back_size = size;
}

/* submit recv buffer and return msg (to wait for reply).
   min_size allows to receive less data. 0=size */
sockio_msg_t *sockio_recv(sockio_handle_t *sio, APTR buf, ULONG max_size, ULONG min_size)
{
  if(sio->state != SOCKIO_STATE_OK) {
    LOG(("sockio: wrong state for recv!\n"));
    return NULL;
  }

  sockio_msg_t *msg = alloc_msg(sio, SOCKIO_MSG_RECV, buf, max_size, min_size);
  if(msg == NULL) {
    LOG(("sockio: no recv msg!\n"));
    return NULL;
  }

  LOG(("sockio: recv put msg %lx (buf=%lx, max_size=%ld, min_size=%ld)\n",
    msg, buf, max_size, min_size));
  PutMsg(sio->msg_port, (struct Message *)msg);
  return msg;
}

/* submit send buffer and return msg (to wait for reply) */
sockio_msg_t *sockio_send(sockio_handle_t *sio, APTR buf, ULONG size)
{
  if(sio->state != SOCKIO_STATE_OK) {
    LOG(("sockio: wrong state for send!\n"));
    return NULL;
  }

  sockio_msg_t *msg = alloc_msg(sio, SOCKIO_MSG_SEND, buf, size, size);
  if(msg == NULL) {
    LOG(("sockio: no send msg!\n"));
    return NULL;
  }

  LOG(("sockio: send put msg %lx (buf=%lx, size=%ld)\n",
    msg, buf, size));
  PutMsg(sio->msg_port, (struct Message *)msg);
  return msg;
}

/* submit wait char message */
sockio_msg_t *sockio_wait_char(sockio_handle_t *sio, ULONG timeout_us)
{
  if(sio->state != SOCKIO_STATE_OK) {
    LOG(("sockio: wrong state for wait char!\n"));
    return NULL;
  }

  sockio_msg_t *msg = alloc_msg(sio, SOCKIO_MSG_WAIT_CHAR, NULL, 0, 0);
  if(msg == NULL) {
    LOG(("sockio: no wait char msg!\n"));
    return NULL;
  }

  // split timeout in secs
  ULONG secs = 0;
  if(timeout_us > 1000000UL) {
    secs = timeout_us / 1000000UL;
    timeout_us %= 1000000UL;
  }

  // keep msg as user_data and fire up timer
  timer_job_t *job = timer_start(sio->timer, secs, timeout_us, msg);
  LOG(("sockio: TIMER start job %lx\n", job));

  // keep timeout in "min_size"
  msg->min_size = timeout_us;
  // buffer->data is used for timer job
  msg->buffer.data = (UBYTE *)job;

  LOG(("sockio: send wait char msg %lx (timeout_us=%ld)\n",
    msg, timeout_us));
  PutMsg(sio->msg_port, (struct Message *)msg);
  return msg;
}

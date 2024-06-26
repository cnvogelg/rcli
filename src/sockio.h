#ifndef SOCKIO_H
#define SOCKIO_H

#include "buf.h"

struct sockio_msg
{
  struct Message msg;
  APTR           private;
  buf_t          buffer;
  ULONG          min_size;
  UBYTE          type;
  UBYTE          pad;
};
typedef struct sockio_msg sockio_msg_t;

#define SOCKIO_STATE_OK      0
#define SOCKIO_STATE_EOF     1
#define SOCKIO_STATE_ERROR   2

#define SOCKIO_MSG_RECV           0
#define SOCKIO_MSG_SEND           1
#define SOCKIO_MSG_WAIT_CHAR      2

struct sockio_handle;
typedef struct sockio_handle sockio_handle_t;

sockio_handle_t *sockio_init(int socket, struct MsgPort *user_port, ULONG max_msgs);
void sockio_exit(sockio_handle_t *sio);

/* wait for socket io or other signals and return state. sig_mask is in/out param */
ULONG sockio_wait_handle(sockio_handle_t *sio, ULONG *sig_mask);

/* submit recv buffer and return msg (to wait for reply).
   min_size allows to receive less data. 0=size */
sockio_msg_t *sockio_recv(sockio_handle_t *sio, APTR buf, ULONG max_size, ULONG min_size);

/* submit send buffer and return msg (to wait for reply) */
sockio_msg_t *sockio_send(sockio_handle_t *sio, APTR buf, ULONG size);

/* submit wait char message */
sockio_msg_t *sockio_wait_char(sockio_handle_t *sio, ULONG timeout_us);

/* free returned message */
void sockio_free_msg(sockio_handle_t *sio, sockio_msg_t *msg);

#endif
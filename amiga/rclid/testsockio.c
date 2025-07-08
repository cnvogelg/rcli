#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>

#include "serv.h"
#include "sockio.h"

static UBYTE buffer[1024];
static sockio_msg_t *end_msg;
static sockio_msg_t *wait_msg;

static BOOL handle_msg(sockio_handle_t *sio, sockio_msg_t *msg)
{
  BOOL stay = TRUE;
  Printf("test: got msg %lx type=%ld\n", msg, msg->type);

  switch(msg->type) {
  case SOCKIO_MSG_RECV:
    // is EOF?
    if(msg->buffer.size == 0) {
      PutStr("test: recv EOF!\n");
      end_msg = sockio_send(sio, "BYE.\n", 5);
      if(end_msg == NULL) {
        PutStr("test: ERROR setting up tx msg!\n");
        stay = FALSE;
      }
      break;
    }

    // if msg starts with 'q' then end
    if(msg->buffer.data[0] == 'q') {
      Printf("test: QUITTING...\n");
      end_msg = sockio_send(sio, "bye.\n", 5);
      if(end_msg == NULL) {
        PutStr("test: ERROR setting up tx msg!\n");
        stay = FALSE;
      }
      break;
    }

    // if msg starts with 'w' then start wait
    if(wait_msg == NULL) {
      if(msg->buffer.data[0] == 'w') {
        wait_msg = sockio_wait_char(sio, 5000000UL); // wait 5s
        if(wait_msg == NULL) {
          PutStr("test: ERROR starting wait!\n");
        } else {
          Printf("test: waiting for 5s...\n");
        }
        break;
      }
    }
    // reply received msg
    sockio_msg_t *tx_msg = sockio_send(sio, msg->buffer.data, msg->buffer.size);
    if(tx_msg == NULL) {
      PutStr("test: ERROR setting up tx msg!\n");
    } else {
      Printf("test: send %ld bytes\n", msg->buffer.size);
    }
    break;
  case SOCKIO_MSG_SEND:
    if(msg == end_msg) {
        PutStr("test: goodbye!\n");
        stay = FALSE;
        break;
    }
    // start receiving again
    sockio_msg_t *rx_msg = sockio_recv(sio, buffer, 1024, 1);
    if(rx_msg == NULL) {
      PutStr("test: ERROR setting up rx msg!\n");
    } else {
      Printf("test: recv up to %ld bytes\n", rx_msg->buffer.size);
    }
    break;
  case SOCKIO_MSG_WAIT_CHAR: {
    // get result:
    ULONG got_char = msg->buffer.size;
    Printf("test: WAIT CHAR result: got_char=%ld\n", got_char);
    wait_msg = NULL;
    // send reply
    sockio_send(sio, "waited!\n", 8);
    break;
  }
  case SOCKIO_MSG_ERROR: {
    Printf("test: ERROR msg: %ld!\n", msg->min_size);
    stay = FALSE;
    break;
  }
  }

  sockio_free_msg(sio, msg);
  return stay;
}

static int main_loop(sockio_handle_t *sio, struct MsgPort *msg_port)
{
  // allow buf to be filled
  sockio_msg_t *rx_msg = sockio_recv(sio, buffer, 1024, 1);
  if(rx_msg == NULL) {
    PutStr("test: No rx msg\n");
    return RETURN_FAIL;
  }

  ULONG port_mask = 1 << msg_port->mp_SigBit;

  sockio_send(sio, "hello!\n", 7);

  while(1) {
    ULONG sig_mask = SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | port_mask;

    sockio_wait_handle(sio, &sig_mask);
    Printf("test: wait sig mask=%lx\n", sig_mask);

    if(sig_mask & SIGBREAKF_CTRL_D) {
      PutStr("test: Flush!\n");
      sockio_flush(sio);
    }
    if(sig_mask & SIGBREAKF_CTRL_C) {
      PutStr("test: Break!\n");
      break;
    }

    if(sig_mask & port_mask) {
      struct Message *msg;
      BOOL stay = TRUE;
      while((msg = GetMsg(msg_port)) != NULL) {
        BOOL my_stay = handle_msg(sio, (sockio_msg_t *)msg);
        stay = stay && my_stay;
      }
      if(!stay) {
        break;
      }
    }
  }

  return RETURN_OK;
}

int main(void)
{
  int socket = serv_init();
  if(socket == -1) {
    return RETURN_FAIL;
  }

  struct MsgPort *msg_port = CreateMsgPort();
  if(msg_port == NULL) {
    return RETURN_FAIL;
  }

  int result = RETURN_FAIL;
  PutStr("test: setting up sockio...\n");
  sockio_handle_t *sio = sockio_init(socket, msg_port, 8);
  if(sio != NULL) {
    PutStr("test: main loop\n");
    result = main_loop(sio, msg_port);

    PutStr("test: free sockio...\n");
    sockio_exit(sio);
  }
  PutStr("test: done\n");

  DeleteMsgPort(msg_port);

  serv_exit(socket);

  return result;
}

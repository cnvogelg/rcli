#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>

#include <string.h>
#include <sys/ioctl.h>

#ifdef DEBUG_RCLID
#define DEBUG
#endif

#include "log.h"
#include "serv.h"
#include "vcon.h"
#include "shell.h"
#include "sockio.h"
#include "rclid.h"
#include "init.h"

#define ZERO 0

extern struct DosLibrary *DOSBase;
extern struct Library *SocketBase;

void error_out(int socket, const char *msg)
{
  send(socket, msg, strlen(msg), 0);
  LOG(("ERROR_OUT: '%s' errno=%ld\n", msg, Errno()));
}

static serv_data_t *init_rclid(int socket, ULONG shell_stack, ULONG max_msgs, ULONG init_wait_us)
{
  serv_data_t *sd = (serv_data_t *)AllocVec(sizeof(serv_data_t), MEMF_CLEAR | MEMF_ANY);
  if(sd == NULL) {
    error_out(socket, "ERROR allocating seerver data!\n");
    return NULL;
  }

  // set socket lib options
  // disable SIGINT via Ctrl-C. we use sig mask for that
  long err = SocketBaseTags(SBTM_SETVAL(SBTC_BREAKMASK), 0, TAG_END);
  if(err != 0) {
    error_out(socket, "ERROR setting socket.lib options!\n");
    return NULL;
  }

  sd->socket = socket;

  // create sockio port
  sd->sockio_port = CreateMsgPort();
  if(sd->sockio_port == NULL) {
    error_out(socket, "ERROR create sockio port!\n");
    return NULL;
  }
  sd->sockio_port_mask = 1 << sd->sockio_port->mp_SigBit;

  // create sockio
  sockio_handle_t *sockio = sockio_init(socket, sd->sockio_port, max_msgs);
  if(sockio == NULL) {
    error_out(socket, "ERROR opening sockio!\n");
    return NULL;
  }
  sd->sockio = sockio;

  // how long to wait for init hand shake
  sd->init_wait_us = init_wait_us;
  sd->max_msgs = max_msgs;
  sd->shell_stack = shell_stack;

  return sd;
}

static BOOL start_shell(serv_data_t *sd)
{
  // create vcon port
  sd->vcon_port = CreateMsgPort();
  if(sd->vcon_port == NULL) {
    error_out(sd->socket, "ERROR create vcon port!\n");
    return FALSE;
  }
  sd->vcon_port_mask = 1 << sd->vcon_port->mp_SigBit;

  // setup virtual console for this sesion
  vcon_handle_t *vcon = vcon_init(sd->vcon_port, sd->max_msgs);
  if(vcon == NULL) {
    error_out(sd->socket, "ERROR opening virtual console!\n");
    return FALSE;
  }
  sd->vcon = vcon;

  // create shell and use console
  BPTR fh = vcon_create_fh(vcon);
  shell_handle_t *sh = shell_init(fh, ZERO, NULL, sd->shell_stack);
  if(sh == NULL) {
    error_out(sd->socket, "ERROR opening shell!\n");
    return FALSE;
  }
  sd->shell = sh;

  return TRUE;
}

static void exit_rclid(serv_data_t *sd)
{
  if(sd == NULL) {
    return;
  }

  if(sd->sockio != NULL) {
    sockio_exit(sd->sockio);
  }

  if(sd->sockio_port != NULL) {
    DeleteMsgPort(sd->sockio_port);
  }

  if(sd->shell != NULL) {
    BYTE result = shell_exit(sd->shell);
    LOG(("rclid: shell result %ld\n", (LONG)result));
  }

  if(sd->vcon != NULL) {
    vcon_exit(sd->vcon);
    sd->vcon = NULL;
  }

  if(sd->vcon_port != NULL) {
    DeleteMsgPort(sd->vcon_port);
  }
}

#define HANDLE_OK     0
#define HANDLE_ERROR  1
#define HANDLE_END    2

static int handle_vcon_msg(serv_data_t *sd, vcon_msg_t *vmsg, BOOL socket_active)
{
  buf_t *buffer = &vmsg->buffer;
  LOG(("rclid: handle vcon msg: type=%ld pkt=%lx data=%lx size=%ld\n",
    (LONG)vmsg->type, vmsg->private, buffer->data, buffer->size));
  int result = HANDLE_OK;
  BOOL reply = TRUE;

  switch(vmsg->type) {
  case VCON_MSG_WRITE: {
    // forward to sockio
    if(socket_active) {
      sockio_msg_t *msg = sockio_send(sd->sockio, buffer->data, buffer->size);
      if(msg == NULL) {
        error_out(sd->socket, "Error in sockio_send!\n");
        result = HANDLE_ERROR;
      } else {
        LOG(("rclid: got vcon write msg=%lx -> sockio: msg=%lx\n", vmsg, msg));
        // remember associated vmsg (to reply later)
        msg->user_data = vmsg;
        reply = FALSE;
      }
    } else {
      LOG(("rclid: slurp vcon write!\n"));
    }
    break;
  }

  case VCON_MSG_READ: {
    // start receiving from sockio (at least a byte)
    if(socket_active) {
      sockio_msg_t *msg = sockio_recv(sd->sockio, buffer->data, buffer->size, 1);
      if(msg == NULL) {
        error_out(sd->socket, "Error in sockio_recv!\n");
        result = HANDLE_ERROR;
      } else {
        LOG(("rclid: got vcon read msg=%lx -> sockio: msg=%lx\n", vmsg, msg));
        // remember associated vmsg (to reply later)
        msg->user_data = vmsg;
        reply = FALSE;
      }
    } else {
      LOG(("rclid: slurp vcon read!\n"));
    }
    break;
  }

  case VCON_MSG_BUFFER_MODE: {
    LOG(("rclid: BUFFER MODE=%ld\n", (LONG)vmsg->buffer_mode));
    if(socket_active) {
      // prepare our own CSI to report the buffer mode
      UBYTE *buf = sd->csi_buf;
      buf[0] = 0x9b; // CSI
      buf[1] = (UBYTE)vmsg->buffer_mode + '0';
      buf[2] = 'V';
      sockio_msg_t *msg = sockio_send(sd->sockio, buf, 3);
      if(msg == NULL) {
        error_out(sd->socket, "Error in sockio_send buffer mode!\n");
        result = HANDLE_ERROR;
      } else {
        LOG(("rclid: got vcon write msg=%lx -> sockio: msg=%lx\n", vmsg, msg));
        // remember associated vmsg (to reply later)
        msg->user_data = vmsg;
        reply = FALSE;
      }
    } else {
      LOG(("rclid: slurp vcon buffer mode!\n"));
    }
    break;
  }

  case VCON_MSG_WAIT_CHAR: {
    if(socket_active) {
      ULONG timeout_us = buffer->size;
      // start wait char in sockiot
      sockio_msg_t *msg = sockio_wait_char(sd->sockio, timeout_us);
      if(msg == NULL) {
        error_out(sd->socket, "Error in sockio_wait_char!\n");
        result = HANDLE_ERROR;
      } else {
        LOG(("rclid: got vcon wait char msg=%lx -> sockio: msg=%lx (timeout=%ld)\n", vmsg, msg, timeout_us));
        // remember associated vmsg (to reply later)
        msg->user_data = vmsg;
        reply = FALSE;
      }
    } else {
      LOG(("rclid: slurp vcon wait char!\n"));
    }
    break;
  }

  case VCON_MSG_END:
    LOG(("rclid: got vcon end msg\n"));
    result = HANDLE_END;
    break;

  default:
    LOG(("rclid: UNKNOWN vcon msg: %ld\n", (ULONG)vmsg->type));
    break;
  }

  if(reply) {
    LOG(("rclid: reply vmsg %lx\n", vmsg));
    ReplyMsg((struct Message *)vmsg);
  }

  return result;
}

static int handle_sockio_msg(serv_data_t *sd, sockio_msg_t *msg)
{
  LOG(("rclid: handle sockio msg: type=%ld\n", (LONG)msg->type));
  buf_t *buffer = &msg->buffer;
  vcon_msg_t *vmsg = (vcon_msg_t *)msg->user_data;
  int result = HANDLE_OK;

  switch(msg->type) {
  case SOCKIO_MSG_RECV:
    LOG(("rclid: sockio recv done: msg=%lx -> vmsg=%lx (size=%ld)\n", msg, vmsg, buffer->size));
    vmsg->buffer.size = buffer->size;
    break;
  case SOCKIO_MSG_SEND:
    LOG(("rclid: sockio send done: msg=%lx -> vmsg=%lx (size=%ld)\n", msg, vmsg, buffer->size));
    break;
  case SOCKIO_MSG_WAIT_CHAR: {
    // get result:
    ULONG got_char = buffer->size;
    LOG(("rclid: sockio wait_char done: msg=%lx -> vmsg=%lx (got_char=%ld)\n", msg, vmsg, got_char));
    vmsg->buffer.size = got_char;
    break;
  }
  case SOCKIO_MSG_END:
    LOG(("rclid: sockio ended.\n"));
    result = HANDLE_END;
    break;
  case SOCKIO_MSG_ERROR:
    LOG(("rclid: sockio ERROR!\n"));
    result = HANDLE_ERROR;
    break;
  default:
    LOG(("rclid: unknown sockio msg: %ld\n", msg->type));
    break;
  }

  // finish sockio msg
  sockio_free_msg(sd->sockio, msg);

  // if a vmsg was attached then reply it
  if(vmsg != NULL) {
    LOG(("rclid: reply vmsg %lx\n", vmsg));
    ReplyMsg((struct Message *)vmsg);
  }

  LOG(("rclid: done sockio msg. result=%ld\n", result));
  return result;
}

static void main_loop(serv_data_t *sd)
{
  ULONG shell_mask = shell_exit_mask(sd->shell);
  ULONG vcon_mask = vcon_get_sigmask(sd->vcon);
  ULONG vport_mask = sd->vcon_port_mask;
  ULONG sport_mask = sd->sockio_port_mask;
  ULONG masks = shell_mask | vcon_mask | vport_mask | sport_mask | SIGBREAKF_CTRL_C;
  BOOL socket_active = TRUE;
  BOOL vcon_active = TRUE;

  LOG(("rclid: enter main\n"));
  while(socket_active || vcon_active) {

    // wait/select/handle sockio
    ULONG sig_mask = masks;
    LOG(("rclid: wait socket begin: sig_mask=%lx\n", sig_mask));
    sockio_wait_handle(sd->sockio, &sig_mask);
    LOG(("rclid: wait socket return: got sig_mask=%lx\n", sig_mask));

    // do vcon
    if(vcon_mask & sig_mask) {
      LOG(("rclid: vcon mask\n"));
      BOOL ok = vcon_handle_sigmask(sd->vcon, sig_mask);
      if(!ok) {
        // error means vcon had some trouble allocating memory
        error_out(sd->socket, "Error: out of memory!\n");
        sockio_end(sd->sockio);
      }
    }

    // incoming sockio msg
    if(sport_mask & sig_mask) {
      LOG(("rclid: sockio incoming msgs\n"));
      struct Message *msg;
      while((msg = GetMsg(sd->sockio_port)) != NULL) {
        int result = handle_sockio_msg(sd, (sockio_msg_t *)msg);
        if(result == HANDLE_ERROR) {
          LOG(("rclid: sockio ended with ERROR!\n"));
          socket_active = FALSE;
        }
        else if(result == HANDLE_END) {
          LOG(("rclid: sockio ended."));
          socket_active = FALSE;
        }
      }
    }

    // incoming vcon message
    if(vport_mask & sig_mask) {
      LOG(("rclid: vcon incoming msgs\n"));
      struct Message *msg;
      while((msg = GetMsg(sd->vcon_port)) != NULL) {
        int result = handle_vcon_msg(sd, (vcon_msg_t *)msg, socket_active);
        // error means submitting to sockio had a problem
        // vcon message will be replied without any processing
        // and will lead to vcon closing.
        if(result == HANDLE_ERROR) {
          LOG(("rclid: vcon reports sockio error\n"));
        }
        // vcon is reported done
        else if(result == HANDLE_END) {
          LOG(("rclid: vcon is done.\n"));
          vcon_active = FALSE;
          // shut down socket
          if(socket_active) {
            LOG(("rclid: shutdown sockio\n"));
            sockio_end(sd->sockio);
          }
        }
      }
    }

  }
  LOG(("rclid: leave main\n"));
}

int main(void)
{
  int socket = serv_init();
  if(socket == -1) {
    return RETURN_FAIL;
  }

  serv_data_t *sd = init_rclid(socket, 8192, 8, 900000UL);
  if(sd != NULL) {
    if(init_loop(sd)) {
      if(start_shell(sd)) {
        main_loop(sd);
      } else {
        error_out(sd->socket, "Error starting shell!\n");
      }
    }
  }
  exit_rclid(sd);

  FreeVec(sd);

  serv_exit(socket);

  return RETURN_OK;
}

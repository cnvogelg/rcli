#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>

#include <string.h>
#include <sys/ioctl.h>

#ifdef DEBUG_INIT
#define DEBUG
#endif

#include "log.h"
#include "sockio.h"
#include "vcon.h"
#include "shell.h"
#include "rclid.h"
#include "init.h"

#define INIT_STATE_WAIT   0
#define INIT_STATE_RECV   1
#define INIT_STATE_SEND   2

BOOL init_loop(serv_data_t *sd)
{
  LOG(("rclid: init begin\n"));
  int state = INIT_STATE_WAIT;
  BOOL ok = TRUE;
  BOOL stay = TRUE;
  sockio_msg_t *msg = NULL;
  UBYTE *seq = sd->handshake_buf;

  // start wait
  msg = sockio_wait_char(sd->sockio, sd->init_wait_us);
  if(msg == NULL) {
    error_out(sd->socket, "Error waiting!");
    return FALSE;
  }

  // init loop
  while(stay) {
    ULONG mask = sd->sockio_port_mask;
    sockio_wait_handle(sd->sockio, &mask);
    if(mask & sd->sockio_port_mask) {
      struct Message *raw_msg;
      while((raw_msg = GetMsg(sd->sockio_port)) != NULL) {
        sockio_msg_t *smsg = (sockio_msg_t *)raw_msg;
        ULONG type = smsg->type;
        LOG(("rclid: got init sockio msg: %ld\n", type));
        switch(type) {
        case SOCKIO_MSG_WAIT_CHAR:
          // got char
          if(smsg->buffer.size == 1) {
            // epxect init handshake "CLIx"
            seq[4] = 0;
            msg = sockio_recv(sd->sockio, seq, HANDSHAKE_LEN, HANDSHAKE_LEN);
            if(msg == NULL) {
              LOG(("rclid: init recv ERROR!\n"));
              stay = FALSE;
              ok = FALSE;
            }
            state = INIT_STATE_RECV;
          } else {
            // no char
            stay = FALSE;
            sd->flags |= FLAG_PASSIVE;
            LOG(("rclid: no char -> passive!\n"));
          }
          break;
        case SOCKIO_MSG_RECV:
          LOG(("rclid: init got handshake: %s\n", seq));
          if(strncmp(seq, HANDSHAKE_STR, HANDSHAKE_LEN - 1)!=0) {
            LOG(("rclid: wrong handshake -> passive!\n"));
            stay = FALSE;
            sd->flags |= FLAG_PASSIVE;
            // push back data so it won't get lost
            sockio_push_back(sd->sockio, seq, HANDSHAKE_LEN);
          } else {
            // CLI1 = support medium mode
            // CLI0 = no medium mode
            UBYTE mode = seq[3];
            if(mode == '1') {
              sd->flags |= FLAG_MEDIUM;
            }
            // answer
            msg = sockio_send(sd->sockio, seq, HANDSHAKE_LEN);
            if(msg == NULL) {
              LOG(("rclid: init send ERROR!\n"));
              stay = FALSE;
              ok = FALSE;
            }
            state = INIT_STATE_SEND;
          }
          break;
        case SOCKIO_MSG_SEND:
          LOG(("rclid: init sent handshake\n"));
          stay = FALSE;
          break;
        case SOCKIO_MSG_END:
          stay = FALSE;
          ok = FALSE;
          LOG(("rclid: init sockio done\n"));
          break;
        case SOCKIO_MSG_ERROR:
          stay = FALSE;
          ok = FALSE;
          LOG(("rclid: init sockio ERROR\n"));
          break;
        default:
          LOG(("rclid: invalid init sockio msg!\n"));
          break;
        }
        sockio_free_msg(sd->sockio, smsg);
      }
    }
  }

  LOG(("rclid: init done\n"));
  return ok;
}

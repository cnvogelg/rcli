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
#include "run.h"
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
  UBYTE *seq = sd->tmp_rx_buf;

  // from ms to us
  ULONG init_wait_us = sd->options->init_wait * 1000UL;

  // start wait
  msg = sockio_wait_char(sd->sockio, init_wait_us);
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
            // allow at least 1 byte
            msg = sockio_recv(sd->sockio, seq, HANDSHAKE_LEN, 1);
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
        case SOCKIO_MSG_RECV: {
          ULONG size = smsg->buffer.size;
          LOG(("rclid: init got handshake: size=%ld %s\n", size, seq));
          if((size < HANDSHAKE_LEN) || (strncmp(seq, HANDSHAKE_STR, HANDSHAKE_LEN - 1)!=0)) {
            LOG(("rclid: wrong handshake -> passive!\n"));
            stay = FALSE;
            sd->flags |= FLAG_PASSIVE;
            // push back data so it won't get lost
            if(!sockio_push_back(sd->sockio, seq, size)) {
              LOG(("rclid: failed push back!\n"));
              stay = FALSE;
              ok = FALSE;
            }
          } else {
            // CLIx: handshake with flags number
            // x =  0 no flags
            // x |= 1 medium mode
            // x |= 2 signals
            UBYTE flags = seq[3];
            // make sure flags are number
            if((flags < '0') || (flags > '9')) {
              LOG(("rclid: init send wrong flags ERROR!\n"));
              stay = FALSE;
              ok = FALSE;
            } else {
              LOG(("rclid: got flags %x\n", flags));
              flags -= '0';
              sd->flags = 0;
              if(flags & 1) {
                sd->flags |= FLAG_MEDIUM;
              }
              if(flags & 2) {
                sd->flags |= FLAG_SIGNALS;
              }
              // handshake ok: echo handshake back
              msg = sockio_send(sd->sockio, seq, HANDSHAKE_LEN);
              if(msg == NULL) {
                LOG(("rclid: init send ERROR!\n"));
                stay = FALSE;
                ok = FALSE;
              }
              state = INIT_STATE_SEND;
            }
          }
          break;
        }
        case SOCKIO_MSG_SEND:
          LOG(("rclid: init sent handshake\n"));
          stay = FALSE;
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

#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>

#include <string.h>
#include <sys/ioctl.h>

#define LOG_ENABLED

#include "log.h"
#include "serv.h"
#include "vcon.h"
#include "shell.h"

#define ZERO 0

extern struct DosLibrary *DOSBase;
extern struct Library *SocketBase;

struct serv_data {
  int              socket;
  vcon_handle_t   *vcon;
  shell_handle_t  *shell;
};
typedef struct serv_data serv_data_t;

void error_out(int socket, const char *msg)
{
  send(socket, msg, strlen(msg), 0);
  LOG(("ERROR_OUT: '%s' errno=%ld\n", msg, Errno()));
}

static BOOL init_rclid(serv_data_t *sd, int socket, ULONG shell_stack)
{
  // set socket lib options
  // disable SIGINT via Ctrl-C. we use sig mask for that
  long err = SocketBaseTags(SBTM_SETVAL(SBTC_BREAKMASK), 0, TAG_END);
  if(err != 0) {
    error_out(socket, "ERROR setting socket.lib options!\n");
    return FALSE;
  }

  sd->socket = socket;

  // setup virtual console for this sesion
  vcon_handle_t *vcon = vcon_init();
  if(vcon == NULL) {
    error_out(socket, "ERROR opening virtual console!\n");
    return FALSE;
  }
  sd->vcon = vcon;

  // create shell and use console
  BPTR fh = vcon_create_fh(vcon);
  shell_handle_t *sh = shell_init(fh, ZERO, NULL, shell_stack);
  if(sh == NULL) {
    vcon_exit(vcon);
    error_out(socket, "ERROR opening shell!\n");
    return FALSE;
  }
  sd->shell = sh;
}

static void exit_rclid(serv_data_t *sd)
{
  if(sd->vcon != NULL) {
    vcon_exit(sd->vcon);
    sd->vcon = NULL;
  }

  if(sd->shell != NULL) {
    shell_exit(sd->shell);
  }
}

static BOOL main_loop(serv_data_t *sd)
{
  ULONG shell_mask = shell_exit_mask(sd->shell);
  ULONG con_mask = vcon_get_sigmask(sd->vcon);
  ULONG masks = shell_mask | con_mask | SIGBREAKF_CTRL_C;

  BOOL rx_pending = FALSE;
  BOOL tx_pending = FALSE;
  vcon_buf_t rx_buf;
  vcon_buf_t tx_buf;
  fd_set rx_fds;
  fd_set tx_fds;
  ULONG tx_pos = 0;

  while(1) {

    // prepare select
    FD_ZERO(&rx_fds);
    if(rx_pending) {
      FD_SET(sd->socket, &rx_fds);
    }
    FD_ZERO(&tx_fds);
    if(tx_pending) {
      FD_SET(sd->socket, &tx_fds);
    }
    ULONG got_mask = masks;

    // do select and wait
    LOG(("ENTER WAIT: mask=%lx rx_pend=%ld tx_pend=%ld\n", got_mask, (LONG)rx_pending, (LONG)tx_pending));
    long wait_result = WaitSelect(sd->socket + 1, &rx_fds, &tx_fds, NULL, NULL, &got_mask);
    LOG(("LEAVE WAIT: res=%ld mask=%lx\n", wait_result, got_mask));

    if(wait_result == -1) {
      error_out(sd->socket, "WaitSelect failed!\n!");
      return FALSE;
    }
    else if(wait_result > 0) {
      // handle socket RX
      if(FD_ISSET(sd->socket, &rx_fds)) {
        if(rx_pending) {
          LONG size = rx_buf.size;
          LOG(("RX READY! %ld bytes\n", size));
          long res = recv(sd->socket, rx_buf.data, size, 0);
          LOG(("RX RESULT: %ld\n", res));
          if(res == -1) {
            if(Errno() != EAGAIN) {
              error_out(sd->socket, "RX error!\n");
              return FALSE;
            } else {
              LOG(("RX: again!\n"));
            }
          }
          else if(res > 0) {
            for(int i=0;i<res;i++) {
              LOG(("%02lx ", (ULONG)rx_buf.data[i]));
            }
            LOG(("RX DONE!\n"));

            rx_buf.size = res;
            vcon_read_end(sd->vcon, &rx_buf);
            rx_pending = FALSE;
          }
          else {
            LOG(("RX EOF!\n"));
            return TRUE;
          }
        } else {
          LOG(("RX READY but not pending?!\n"));
        }
      }
      // handle socket TX
      if(FD_ISSET(sd->socket, &tx_fds)) {
        if(tx_pending) {
          ULONG size = tx_buf.size - tx_pos;
          LOG(("TX READY! send %ld@%ld bytes\n", size, tx_pos));
          long res = send(sd->socket, tx_buf.data + tx_pos, size, 0);
          LOG(("TX RESULT: %ld\n", res));
          if(res == -1) {
            if(Errno() != EAGAIN) {
              error_out(sd->socket, "TX error!\n");
              return FALSE;
            } else {
              LOG(("TX: again!\n"));
            }
          }
          else if(res > 0) {
            tx_pos += res;
            if(tx_pos == tx_buf.size) {
              LOG(("TX DONE!\n"));
              tx_pending = FALSE;
              vcon_write_end(sd->vcon, &tx_buf);
            }
          }
          else {
            LOG(("TX EOF!\n"));
            return TRUE;
          }
        } else {
          LOG(("TX READY but not pending?!\n"));
        }
      }
    }
    else if(wait_result == 0) {
      // dispatch signals
      ULONG got_con_mask = got_mask & con_mask;
      if(got_con_mask) {
        ULONG status = vcon_handle_sigmask(sd->vcon, got_con_mask);
        LOG(("VCON handle: mask=%lx -> status=%lx\n", got_con_mask, status));

        // dispatch vcon status
        if(status & VCON_HANDLE_READ) {
          // not pending?
          if(!rx_pending) {
            // start reading into rx buf
            BOOL ok = vcon_read_begin(sd->vcon, &rx_buf);
            if(!ok) {
              error_out(sd->socket, "ERROR: vcon_read_begin!\n");
              return FALSE;
            }
            rx_pending = TRUE;
            LOG(("VREAD: pending rx: size=%ld\n", rx_buf.size));
          } else {
            LOG(("VREAD: already rx pending!\n"));
          }
        }
        if(status & VCON_HANDLE_WRITE) {
          // not pending?
          if(!tx_pending) {
            // start writing into tx_buf
            BOOL ok = vcon_write_begin(sd->vcon, &tx_buf);
            if(!ok) {
              error_out(sd->socket, "ERROR: vcon_write_begin!\n");
              return FALSE;
            }
            tx_pending = TRUE;
            tx_pos = 0;
            LOG(("VWRITE: pending tx: size=%ld\n", tx_buf.size));
          } else {
            LOG(("VWRITE: already tx pending!\n"));
          }
        }
        if(status & VCON_HANDLE_WAIT_CHAR) {
          LOG(("WAIT_CHAR: TBD!\n"));
        }
        if(status & VCON_HANDLE_CLOSE) {
          LOG(("CLOSE!\n"));
        }
      }
      if(got_mask & shell_mask) {
        LOG(("SHELL EXIT!\n"));
        break;
      }
      if(got_mask & SIGBREAKF_CTRL_C) {
        PutStr("*BREAK!\n");
        return FALSE;
      }
    }
  }

  return TRUE;
}

void serv_main(int socket)
{
  serv_data_t sd;
  BOOL ok = init_rclid(&sd, socket, 8192);
  if(!ok) {
    return;
  }

  ok = main_loop(&sd);
  if(!ok) {
    LOG(("DROPPING vcon!\n"));
    vcon_drop(sd.vcon);
  }

  exit_rclid(&sd);
}

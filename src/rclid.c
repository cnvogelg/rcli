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
#include "sockio.h"

#define ZERO 0

extern struct DosLibrary *DOSBase;
extern struct Library *SocketBase;

struct serv_data {
  int              socket;
  vcon_handle_t   *vcon;
  shell_handle_t  *shell;
  sockio_handle_t *sockio;

  BOOL             rx_pending;
  BOOL             tx_pending;

  buf_t            rx_buf;
  buf_t            tx_buf;
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

  // create sockio
  sockio_handle_t *sockio = sockio_init(socket);
  if(sockio == NULL) {
    vcon_exit(vcon);
    shell_exit(sh);
    error_out(socket, "ERROR opening sockio!\n");
    return FALSE;
  }
  sd->sockio = sockio;
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

  if(sd->sockio != NULL) {
    sockio_exit(sd->sockio);
  }
}

#define HANDLE_OK     0
#define HANDLE_ERROR  1
#define HANDLE_END    2

static int handle_vcon(serv_data_t *sd, ULONG got_con_mask)
{
  ULONG status = vcon_handle_sigmask(sd->vcon, got_con_mask);
  LOG(("VCON handle: mask=%lx -> status=%lx\n", got_con_mask, status));

  // console wants to read
  if(status & VCON_HANDLE_READ) {
    // not pending?
    if(!sd->rx_pending) {
      // start reading into rx buf
      BOOL ok = vcon_read_begin(sd->vcon, &sd->rx_buf);
      if(!ok) {
        error_out(sd->socket, "ERROR: vcon_read_begin!\n");
        return HANDLE_ERROR;
      }

      // start sockio read
      ok = sockio_rx_begin(sd->sockio, &sd->rx_buf, 1); // at least 1 char
      if(!ok) {
        error_out(sd->socket, "ERROR: sockio_rx_begin!\n");
        return HANDLE_ERROR;
      }

      sd->rx_pending = TRUE;

      LOG(("VREAD: pending rx: size=%ld\n", sd->rx_buf.size));
    } else {
      LOG(("VREAD: already rx pending!\n"));
    }
  }

  // console wants to write
  if(status & VCON_HANDLE_WRITE) {
    // not pending?
    if(!sd->tx_pending) {
      // start writing into tx_buf
      BOOL ok = vcon_write_begin(sd->vcon, &sd->tx_buf);
      if(!ok) {
        error_out(sd->socket, "ERROR: vcon_write_begin!\n");
        return HANDLE_ERROR;
      }

      // start sockio write
      ok = sockio_tx_begin(sd->sockio, &sd->tx_buf);
      if(!ok) {
        error_out(sd->socket, "ERROR: sockio_tx_begin!\n");
        return HANDLE_ERROR;
      }

      sd->tx_pending = TRUE;

      LOG(("VWRITE: pending tx: size=%ld\n", sd->tx_buf.size));
    } else {
      LOG(("VWRITE: already tx pending!\n"));
    }
  }
  if(status & VCON_HANDLE_WAIT_BEGIN) {
    LOG(("VWAIT_CHAR: begin.\n"));
    sockio_wait_char_begin(sd->sockio);
  }
  if(status & VCON_HANDLE_WAIT_END) {
    LOG(("VWAIT_CHAR: end.\n"));
    sockio_wait_char_end(sd->sockio);
  }
  if(status & VCON_HANDLE_CLOSE) {
    LOG(("VCLOSE!\n"));
    return HANDLE_END;
  }

  return HANDLE_OK;
}


static BOOL main_loop(serv_data_t *sd)
{
  ULONG shell_mask = shell_exit_mask(sd->shell);
  ULONG con_mask = vcon_get_sigmask(sd->vcon);
  ULONG masks = shell_mask | con_mask | SIGBREAKF_CTRL_C;

  while(1) {

    // wait/select/handle sockio
    ULONG sig_mask = masks;
    ULONG flags = sockio_wait_handle(sd->sockio, &sig_mask);
    LOG(("SOCKIO: handle flags=%lx\n", flags));

    // sockio I/O error
    if(flags & SOCKIO_HANDLE_ERROR) {
      error_out(sd->socket, "IO error!\n");
      return FALSE;
    }

    // sockio detected EOF
    if(flags & SOCKIO_HANDLE_EOF) {
      LOG(("SOCKIO: EOF!\n"));
      break;
    }

    // sockio got signal
    if(flags & SOCKIO_HANDLE_SIG_MASK) {
      // handle signals
      if(sig_mask & shell_mask) {
        LOG(("SHELL EXIT!\n"));
      }
      if(sig_mask & SIGBREAKF_CTRL_C) {
        PutStr("*BREAK!\n");
        return FALSE;
      }
      if(sig_mask & con_mask) {
        int res = handle_vcon(sd, sig_mask & con_mask);
        if(res == HANDLE_ERROR) {
          return FALSE;
        } else if(res == HANDLE_END) {
          break;
        }
      }
    }

    // sockio rx req is done
    if(flags & SOCKIO_HANDLE_RX_DONE) {
      LONG rx_size = sockio_rx_end(sd->sockio);
      LOG(("SOCKIO: RX done=%ld\n", rx_size));

      // submit to vcon
      vcon_read_end(sd->vcon, &sd->rx_buf, rx_size);

      sd->rx_pending = FALSE;
    }

    // sockio tx req is done
    if(flags & SOCKIO_HANDLE_TX_DONE) {
      sockio_tx_end(sd->sockio);
      LOG(("SOCKIO: TX done=%ld\n", sd->tx_buf.size));

      // submit to vcon
      vcon_write_end(sd->vcon, &sd->tx_buf);

      sd->tx_pending = FALSE;
    }

    // sockio got char
    if(flags & SOCKIO_HANDLE_WAIT_CHAR) {
      LOG(("SOCKIO: got char!\n"));
      vcon_wait_char_report(sd->vcon);
    }

  }

  return TRUE;
}

int main(void)
{
  int socket = serv_init();
  if(socket == -1) {
    return RETURN_FAIL;
  }

  serv_data_t *sd = (serv_data_t *)AllocVec(sizeof(serv_data_t), MEMF_CLEAR | MEMF_ANY);
  if(sd == NULL) {
    return RETURN_FAIL;
  }

  BOOL ok = init_rclid(sd, socket, 8192);
  if(!ok) {
    return RETURN_FAIL;
  }

  ok = main_loop(sd);
  if(!ok) {
    LOG(("DROPPING vcon!\n"));
    vcon_drop(sd->vcon);
  }

  exit_rclid(sd);

  FreeVec(sd);

  serv_exit(socket);

  return RETURN_OK;
}

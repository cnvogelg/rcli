#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>

#include "serv.h"
#include "sockio.h"

static UBYTE buffer[1024];

static void main_loop(sockio_handle_t *sio)
{
  buf_t buf = {
    .data = buffer,
    .size = 1024,
    .capacity = 1024
  };

  // allow buf to be filled
  sockio_rx_begin(sio, &buf, 1);

  while(1) {
    ULONG sig_mask = SIGBREAKF_CTRL_C;
    ULONG flags = sockio_wait_handle(sio, &sig_mask);
    Printf("Flags=%lx\n", flags);
    if(flags & SOCKIO_HANDLE_ERROR) {
      PutStr("ERROR!\n");
      break;
    }
    if(flags & SOCKIO_HANDLE_EOF) {
      PutStr("EOF!\n");
      break;
    }
    if(flags & SOCKIO_HANDLE_SIG_MASK) {
      Printf("SigMask: %lx\n", sig_mask);
      break;
    }
    if(flags & SOCKIO_HANDLE_RX_DONE) {
      buf_t *buf = sockio_rx_end(sio);
      Printf("RX done=%ld\n", buf->size);

      // echo data back
      sockio_tx_begin(sio, buf);
    }
    if(flags & SOCKIO_HANDLE_TX_DONE) {
      PutStr("TX done\n");
      buf_t *buf = sockio_tx_end(sio);

      // start rx again
      buf->size = buf->capacity;
      sockio_rx_begin(sio, buf, 1);
    }
  }
}

int main(void)
{
  int socket = serv_init();
  if(socket == -1) {
    return RETURN_FAIL;
  }

  PutStr("setting up sockio...\n");
  sockio_handle_t *sio = sockio_init(socket);
  if(sio != NULL) {
    PutStr("main loop\n");
    main_loop(sio);

    PutStr("free sockio...\n");
    sockio_exit(sio);
  }
  PutStr("done\n");

  serv_exit(socket);

  return RETURN_OK;
}

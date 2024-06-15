#include <exec/exec.h>

#include <proto/exec.h>
#include <proto/socket.h>

#include <sys/ioctl.h>
#include <netinet/tcp.h>

//#define LOG_ENABLED
#include "log.h"
#include "sockio.h"

struct sockio_handle {
  int socket;

  buf_t *rx_buffer;
  buf_t *tx_buffer;

  ULONG rx_min_size;

  ULONG rx_actual;
  ULONG tx_actual;

  BOOL  waiting_for_char;
  BOOL  got_char;
};

sockio_handle_t *sockio_init(int socket)
{
  sockio_handle_t *sio = (sockio_handle_t *)AllocVec(sizeof(sockio_handle_t), MEMF_ANY | MEMF_CLEAR);
  if(sio == NULL) {
    return NULL;
  }

  sio->socket = socket;

  // make socket non blocking
  ULONG yes=TRUE;
  IoctlSocket(socket, FIONBIO, (char *)&yes);

  // no delay
  setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(ULONG));

  return sio;
}

void sockio_exit(sockio_handle_t *sio)
{
  if(sio == NULL) {
    return;
  }

  FreeVec(sio);
}

/* wait for socket io or other signals and return SOCKIO_HANDLE flags */
ULONG sockio_wait_handle(sockio_handle_t *sio, ULONG *sig_mask)
{
  fd_set rx_fds;
  fd_set tx_fds;
  buf_t *rx_buf = sio->rx_buffer;
  buf_t *tx_buf = sio->tx_buffer;

  // prepare select

  // do we need to receive something?
  BOOL rx_pending = FALSE;
  FD_ZERO(&rx_fds);
  if((rx_buf != NULL) && (sio->rx_actual < sio->rx_min_size)) {
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

  // wait char handling
  if(sio->waiting_for_char && !sio->got_char) {
    FD_SET(sio->socket, &rx_fds);
  }

  // pre-fill sigmask
  ULONG got_mask = 0;
  if(sig_mask != NULL) {
    got_mask = *sig_mask;
  }

  // do select and wait
  LOG(("ENTER WAIT: mask=%lx rx_pend=%ld tx_pend=%ld\n", got_mask, (LONG)rx_pending, (LONG)tx_pending));
  long wait_result = WaitSelect(sio->socket + 1, &rx_fds, &tx_fds, NULL, NULL, &got_mask);
  LOG(("LEAVE WAIT: res=%ld mask=%lx\n", wait_result, got_mask));

  // we got an error
  if(wait_result == -1) {
    return SOCKIO_HANDLE_ERROR;
  }
  // some signal was hit
  else if(wait_result == 0) {
    *sig_mask = got_mask;
    return SOCKIO_HANDLE_SIG_MASK;
  }

  ULONG flags = 0;

  // handle wait char
  if(sio->waiting_for_char) {
    if(FD_ISSET(sio->socket, &rx_fds) && !sio->got_char) {
      sio->got_char = TRUE;
      LOG(("WAIT CHAR: got it!\n"));
    }
    if(sio->got_char) {
      flags |= SOCKIO_HANDLE_WAIT_CHAR;
    }
  }

  // handle socket RX
  if(rx_buf != NULL) {
    if(FD_ISSET(sio->socket, &rx_fds) && rx_pending) {
      // try to read in missing bytes
      ULONG rx_size = rx_buf->size - sio->rx_actual;
      UBYTE *ptr = rx_buf->data + sio->rx_actual;
      LOG(("RX READY! recv up to %ld bytes (actual %ld)\n", rx_size, sio->rx_actual));
      long res = recv(sio->socket, ptr, rx_size, 0);
      LOG(("RX RESULT: %ld\n", res));
      if(res == -1) {
        // on EAGAIN simply repeat in next handle call
        if(Errno() != EAGAIN) {
          LOG(("RX ERROR: errno=%ld\n", Errno()));
          return SOCKIO_HANDLE_ERROR;
        }
      }
      else if(res > 0) {
        // adjust buffer
        sio->rx_actual += res;
        LOG(("RX DATA: new size=%ld\n", sio->rx_actual));
      }
      else {
        LOG(("RX EOF!\n"));
        flags |= SOCKIO_HANDLE_EOF;
      }
    }

    // is rx done?
    if(sio->rx_actual >= sio->rx_min_size) {
      flags |= SOCKIO_HANDLE_RX_DONE;
    }
  }

  // handle socket TX
  if(tx_buf != NULL) {
    if(FD_ISSET(sio->socket, &tx_fds) && tx_pending) {
      UBYTE *ptr = tx_buf->data + sio->tx_actual;
      ULONG tx_size = tx_buf->size - sio->tx_actual;
      LOG(("TX READY! send %ld bytes (actual %ld)\n", tx_size, sio->tx_actual));
      long res = send(sio->socket, ptr, tx_size, 0);
      LOG(("TX RESULT: %ld\n", res));
      if(res == -1) {
        // on EAGAIN try in next handle call
        if(Errno() != EAGAIN) {
          LOG(("TX ERROR: errno=%ld\n", Errno()));
          return SOCKIO_HANDLE_ERROR;
        }
      }
      else if(res > 0) {
        sio->tx_actual += res;
        LOG(("TX_DATA: actual=%ld size=%ld\n", sio->tx_actual, tx_buf->size));
      }
      else {
        LOG(("TX EOF!\n"));
        flags |= SOCKIO_HANDLE_EOF;
      }
    }

    // is tx done?
    if(tx_buf->size == sio->tx_actual) {
      flags |= SOCKIO_HANDLE_TX_DONE;
    }
  }

  return flags;
}

/* submit RX buffer and receive up to buf->size bytes. */
BOOL sockio_rx_begin(sockio_handle_t *sio, buf_t *buf, ULONG min_size)
{
  // already a buffer pending?
  if(sio->rx_buffer != NULL) {
    return FALSE;
  }

  if(min_size == 0) {
    min_size = buf->size;
  }

  sio->rx_buffer = buf;
  sio->rx_min_size = min_size;
  sio->rx_actual = 0;

  return TRUE;
}

/* after RX_DONE reclaim buffer and return actual size */
buf_t *sockio_rx_end(sockio_handle_t *sio)
{
  if(sio->rx_buffer == NULL) {
    return 0;
  }

  buf_t *buf = sio->rx_buffer;
  buf->size = sio->rx_actual;

  sio->rx_buffer = NULL;
  sio->rx_min_size = 0;
  sio->rx_actual = 0;

  return buf;
}

/* --- TX --- */
/* submit buffer with given size */
BOOL sockio_tx_begin(sockio_handle_t *sio, buf_t *buf)
{
  // already a buffer pending?
  if(sio->tx_buffer != NULL) {
    return FALSE;
  }

  sio->tx_buffer = buf;
  sio->tx_actual = 0;

  return TRUE;
}

/* after TX_DONE reclaim buffer */
buf_t *sockio_tx_end(sockio_handle_t *sio)
{
  buf_t *buf = sio->tx_buffer;

  sio->tx_buffer = NULL;
  sio->tx_actual = 0;

  return buf;
}

/* start waiting for a char */
BOOL sockio_wait_char_begin(sockio_handle_t *sio)
{
  if(sio->waiting_for_char) {
    return FALSE;
  }
  sio->waiting_for_char = TRUE;
  sio->got_char = FALSE;
  return TRUE;
}

/* got a HANDLE_GOT_CHAR and confirm */
void sockio_wait_char_end(sockio_handle_t *sio)
{
  sio->waiting_for_char = FALSE;
  sio->got_char = FALSE;
}


#ifndef SOCKIO_H
#define SOCKIO_H

#include "buf.h"

#define SOCKIO_HANDLE_ERROR       1
#define SOCKIO_HANDLE_EOF         2
#define SOCKIO_HANDLE_RX_DONE     4
#define SOCKIO_HANDLE_TX_DONE     8
#define SOCKIO_HANDLE_SIG_MASK    16
#define SOCKIO_HANDLE_WAIT_CHAR   32

struct sockio_handle;
typedef struct sockio_handle sockio_handle_t;

sockio_handle_t *sockio_init(int socket);
void sockio_exit(sockio_handle_t *sio);

/* wait for socket io or other signals and return SOCKIO_HANDLE flags */
ULONG sockio_wait_handle(sockio_handle_t *sio, ULONG *sig_mask);

/* --- RX --- */
/* submit buffer with given capacity */
BOOL sockio_rx_begin(sockio_handle_t *sio, buf_t *buf, ULONG min_size);

/* after RX_DONE reclaim buffer and return actual size */
ULONG sockio_rx_end(sockio_handle_t *sio);

/* --- TX --- */
/* submit buffer with given size */
BOOL sockio_tx_begin(sockio_handle_t *sio, buf_t *buf);

/* after TX_DONE reclaim buffer */
void sockio_tx_end(sockio_handle_t *sio);

/* --- wait char --- */
/* start waiting for a char */
BOOL sockio_wait_char_begin(sockio_handle_t *sio);

/* got a HANDLE_GOT_CHAR and confirm */
void sockio_wait_char_end(sockio_handle_t *sio);

#endif
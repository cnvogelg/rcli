#ifndef RCLID_H
#define RCLID_H

#define HANDSHAKE_LEN     4
#define HANDSHAKE_STR     "CLI0"

// final options
typedef struct {
  ULONG  stack_size;
  ULONG  max_msgs;
  ULONG  init_wait;
  ULONG  push_back_max;
  ULONG  tmp_buf_size;
  char   *command;
  ULONG  flags;
} serv_options_t;

typedef struct {
  int              socket;
  serv_options_t  *options;

  vcon_handle_t   *vcon;
  run_handle_t    *run;
  sockio_handle_t *sockio;

  struct MsgPort  *sockio_port;
  struct MsgPort  *vcon_port;

  ULONG sockio_port_mask;
  ULONG vcon_port_mask;

  ULONG flags;
  ULONG tmp_rx_size;
  ULONG tmp_tx_size;

  UWORD buffer_mode;

  UBYTE *tmp_rx_buf;
  UBYTE *tmp_tx_buf;
} serv_data_t;

#define FLAG_PASSIVE    1
#define FLAG_MEDIUM     2
#define FLAG_SIGNALS    4
#define FLAG_LINESPLIT  8

extern void error_out(int socket, const char *msg);

#endif

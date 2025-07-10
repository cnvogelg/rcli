#ifndef RCLID_H
#define RCLID_H

#define HANDSHAKE_LEN     4
#define HANDSHAKE_STR     "CLI0"

// final options
typedef struct {
  ULONG  stack_size;
  ULONG  max_msgs;
  ULONG  init_wait;
  char   *command;
} serv_options_t;

typedef struct {
  int              socket;
  serv_options_t  *options;

  vcon_handle_t   *vcon;
  shell_handle_t  *shell;
  sockio_handle_t *sockio;

  struct MsgPort  *sockio_port;
  struct MsgPort  *vcon_port;

  ULONG sockio_port_mask;
  ULONG vcon_port_mask;

  ULONG flags;

  UBYTE csi_buf[4];
  UBYTE handshake_buf[HANDSHAKE_LEN];
} serv_data_t;

#define FLAG_PASSIVE  1
#define FLAG_MEDIUM   2

extern void error_out(int socket, const char *msg);

#endif

#ifndef RCLID_H
#define RCLID_H

#define HANDSHAKE_LEN     4
#define HANDSHAKE_STR     "CLI0"

struct serv_data {
  int              socket;

  vcon_handle_t   *vcon;
  shell_handle_t  *shell;
  sockio_handle_t *sockio;

  struct MsgPort  *sockio_port;
  struct MsgPort  *vcon_port;

  ULONG sockio_port_mask;
  ULONG vcon_port_mask;

  ULONG init_wait_us;
  ULONG flags;
  ULONG max_msgs;
  ULONG shell_stack;

  UBYTE csi_buf[4];
  UBYTE handshake_buf[HANDSHAKE_LEN];
};

typedef struct serv_data serv_data_t;

#define FLAG_PASSIVE  1
#define FLAG_MEDIUM   2

extern void error_out(int socket, const char *msg);

#endif

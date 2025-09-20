#ifndef RCLID_H
#define RCLID_H

// final options
typedef struct {
  ULONG  stack_size;
  ULONG  max_msgs;
  ULONG  init_wait;
  ULONG  push_back_max;
  ULONG  arg_len;
  char   *def_command;
  char   *def_shell_cmd;
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

  ULONG tmp_tx_size;
  UBYTE *tmp_tx_buf;

  // optional arg line passed in by client
  BYTE  *arg_line;
  ULONG  arg_len;

  // keep state of buffer/screen mode
  UBYTE buffer_mode;
  // mode of rcli: shell or command
  UBYTE mode;
  // flags in mode
  UBYTE flags;

} serv_data_t;

#define RCLI_MODE_SHELL   0
#define RCLI_MODE_COMMAND 1

#define RCLI_FLAG_PARSE_SIGNALS   1
#define RCLI_FLAG_SPLIT_LINES     2
#define RCLI_FLAG_DO_MEDIUM_MODE  4
#define RCLI_FLAG_SEND_MODE       8

extern void error_out(int socket, const char *msg);

#endif

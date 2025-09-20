#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>

#include "serv.h"
#include "sockio.h"
#include "line.h"

static int main_loop(sockio_handle_t *sio, struct MsgPort *msg_port)
{
  PutStr("checking for char...\n");
  int result = line_check_char(sio, msg_port, "*", 5000UL);
  Printf("check char result: %ld\n", result);

  if(result >= 0) {
    PutStr("reading line...\n");
    char line[256];
    result = line_read_line(sio, msg_port, line, 256, 5000UL);
    Printf("read line result: %ld\n", result);
    if(result > 0) {
      Printf("got line: '%s'\n", line);
    }
  }

  return RETURN_OK;
}

// own args
static const char *template = SERV_ARGS_TEMPLATE ",MORE/F";
typedef struct {
  serv_params_t serv;
  char *more;
} my_args;
static my_args args;

int main(void)
{
  int socket = serv_init("testsockio", template, (serv_params_t *)&args);
  if(socket == -1) {
    return RETURN_FAIL;
  }
  Printf("got more args: %s\n", args.more);

  struct MsgPort *msg_port = CreateMsgPort();
  if(msg_port == NULL) {
    return RETURN_FAIL;
  }

  int result = RETURN_FAIL;
  PutStr("test: setting up sockio...\n");
  sockio_handle_t *sio = sockio_init(socket, msg_port, 8, 64);
  if(sio != NULL) {
    PutStr("test: main loop\n");
    result = main_loop(sio, msg_port);

    PutStr("test: free sockio...\n");
    sockio_exit(sio);
  }
  PutStr("test: done\n");

  DeleteMsgPort(msg_port);

  serv_exit(socket);

  return result;
}

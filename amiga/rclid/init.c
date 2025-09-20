#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>

#include <string.h>
#include <sys/ioctl.h>

#ifdef DEBUG_INIT
#define DEBUG
#endif

#include "log.h"
#include "sockio.h"
#include "vcon.h"
#include "run.h"
#include "rclid.h"
#include "init.h"
#include "line.h"

#define INIT_STATE_WAIT   0
#define INIT_STATE_RECV   1
#define INIT_STATE_SEND   2

const char *special_chars = "#;";

BOOL init_handshake(serv_data_t *sd)
{
  // how long shall we wait for a first char?
  ULONG wait_ms = sd->options->init_wait;

  LOG(("rclid: init handshake begin. wait=%lu\n", wait_ms));

  // try to get a first command char
  int check_result = line_check_char(sd->sockio, sd->sockio_port, special_chars, wait_ms);
  if(check_result == LINE_RET_ERROR) {
    // error happened
    LOG(("rclid: init check char returned ERROR!"));
    return FALSE;
  }
  else if(check_result == LINE_RET_TIMEOUT) {
    // timeout -> drops into shell
    LOG(("rclid: timeout -> auto drop into shell!"));
    sd->mode = RCLI_MODE_SHELL;
    return TRUE;
  }
  else if(check_result == LINE_RET_OK) {
    // non special char -> drop into command mode
    LOG(("rclid: no special char -> auto drop into command!\n"));
    sd->mode = RCLI_MODE_COMMAND;
  }
  // check for command char
  else if(check_result == '#') {
    // shell mode!
    LOG(("rclid: picked shell mode\n"));
    sd->mode = RCLI_MODE_SHELL;
  }
  else if(check_result == ';') {
    // command mode
    LOG(("rclid: picked command mode\n"));
    sd->mode = RCLI_MODE_COMMAND;
  }
  else {
    LOG(("rclid: invalid mode?!\n"));
    return FALSE;
  }

  // we got a command char. now get a full line until new line afterwards
  ULONG arg_len = sd->options->arg_len;
  LOG(("rclid: read arg line. len=%ld\n", arg_len));
  sd->arg_line = AllocVec(arg_len, MEMF_CLEAR | MEMF_ANY);
  if(sd->arg_line == NULL) {
    LOG(("rclid: no mem for arg line!\n"));
    return FALSE;
  }  

  // now read line
  int read_result = line_read_line(sd->sockio, sd->sockio_port, sd->arg_line, arg_len, wait_ms);

  // error or time out aborts session
  if(read_result < 0) {
    LOG(("rclid: read line error or timeout!\n"));
    return FALSE;
  }

  // if nor char was read then assume arg line is default/null
  if(read_result == 0) {
    FreeVec(sd->arg_line);
    sd->arg_line = NULL; 
    sd->arg_len = 0;
  } else {
    sd->arg_len = read_result;
  }

  LOG(("rclid: init done: arg len=%ld line='%s'\n", sd->arg_len, sd->arg_line));
  return TRUE;
}

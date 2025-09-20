#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>

#include <string.h>
#include <sys/ioctl.h>

#ifdef DEBUG_LINE
#define DEBUG
#endif

#include "log.h"
#include "sockio.h"
#include "line.h"

#define LINE_STATE_WAIT        0
#define LINE_STATE_RECV        1

int line_check_char(sockio_handle_t *sio, struct MsgPort *sockio_port,
    const char *special_chars, ULONG timeout_ms)
{
  LOG(("line: check char\n"));
  int state = LINE_STATE_WAIT;
  int result = LINE_RET_ERROR;
  BOOL stay = TRUE;
  sockio_msg_t *msg = NULL;
  UBYTE buf[2] = {0,0};

  ULONG timeout_us = 1000UL * timeout_ms;

  // start wait
  msg = sockio_wait_char(sio, timeout_us);
  if(msg == NULL) {
    LOG(("line: error waiting!\n"));
    return LINE_RET_ERROR;
  }

  ULONG port_mask = 1 << sockio_port->mp_SigBit;

  // init loop
  while(stay) {
    ULONG mask = port_mask;
    sockio_wait_handle(sio, &mask);
    if(mask & port_mask) {
      struct Message *raw_msg;
      while((raw_msg = GetMsg(sockio_port)) != NULL) {
        sockio_msg_t *smsg = (sockio_msg_t *)raw_msg;
        ULONG type = smsg->type;
        LOG(("line: got sockio msg: %ld\n", type));
        switch(type) {
        case SOCKIO_MSG_WAIT_CHAR:
          // chars are waiting
          if(smsg->buffer.size == 1) {
            // try to read byte
            msg = sockio_recv(sio, &buf, 1, 1);
            if(msg == NULL) {
              LOG(("line: recv ERROR!\n"));
              stay = FALSE;
            }
            state = LINE_STATE_RECV;
          } else {
            // no char
            stay = FALSE;
            result = LINE_RET_TIMEOUT;
            LOG(("line: no char -> timeout!\n"));
          }
          break;
        case SOCKIO_MSG_RECV: {
          ULONG size = smsg->buffer.size;
          LOG(("line: got char: size=%ld %s\n", size, buf));

          // check special char
          result = LINE_RET_OK;
          const char *special = special_chars;
          while(*special) {
            if(*buf == *special) {
                result = *special;
                break;
            }
            special++;
          }

          // if no special char then push it back
          if(result == LINE_RET_OK) {
            LOG(("line: push back: %lx\n", (LONG)buf[0]));
            sockio_push_back(sio, buf, 1);
          }

          stay = FALSE;
          break;
        }
        case SOCKIO_MSG_ERROR:
          stay = FALSE;
          LOG(("line: sockio ERROR\n"));
          break;
        default:
          LOG(("line: invalid sockio msg!\n"));
          break;
        }
        sockio_free_msg(sio, smsg);
      }
    }
  }

  LOG(("line: check done\n"));
  return result;
}

int line_read_line(sockio_handle_t *sio, struct MsgPort *sockio_port,
    APTR buf, ULONG max_size, ULONG timeout_ms)
{
  LOG(("line: read\n"));
  int state = LINE_STATE_WAIT;
  int result = LINE_RET_ERROR;
  BOOL stay = TRUE;
  sockio_msg_t *msg = NULL;

  ULONG timeout_us = 1000UL * timeout_ms;

  // first check if chars are available
  msg = sockio_wait_char(sio, timeout_us);
  if(msg == NULL) {
    LOG(("line: error waiting!\n"));
    return LINE_RET_ERROR;
  }

  ULONG port_mask = 1 << sockio_port->mp_SigBit;
  ULONG left = max_size - 1; // reserve terminating null char
  ULONG got = 0; 

  // read line loop
  while(stay) {
    ULONG mask = port_mask;
    sockio_wait_handle(sio, &mask);
    if(mask & port_mask) {
      struct Message *raw_msg;
      while((raw_msg = GetMsg(sockio_port)) != NULL) {
        sockio_msg_t *smsg = (sockio_msg_t *)raw_msg;
        ULONG type = smsg->type;
        LOG(("line: got init sockio msg: %ld\n", type));
        switch(type) {
        case SOCKIO_MSG_WAIT_CHAR:
          // chars are waiting
          if(smsg->buffer.size == 1) {
            // after waiting try to read
            UBYTE *ptr = (UBYTE *)buf + got;
            msg = sockio_recv(sio, ptr, left, 1);
            if(msg == NULL) {
              LOG(("line: recv ERROR!\n"));
              stay = FALSE;
            }
            state = LINE_STATE_RECV;
          } else {
            // no char
            stay = FALSE;
            result = LINE_RET_TIMEOUT;
            LOG(("line: no char -> timeout!\n"));
          }
          break;
        case SOCKIO_MSG_RECV: {
          ULONG size = smsg->buffer.size;
          UBYTE *ptr = (UBYTE *)buf + got;
          LOG(("line: got char: size=%ld %s\n", size, buf));

          // check if line ends in buf
          ULONG pos = 0;
          while(pos < size) {
            if(*ptr == '\n') {
              break;
            }
            ptr++;
            pos++;
          }

          // found new line?
          if(pos < size) {
            // account new data
            got += pos;
            // terminate line
            *ptr = '\x00';

            LOG(("line: found newline: got %ld bytes\n", got));
            result = got;
            stay = FALSE;
            
            // is something left after new line?
            pos++;
            if(pos < size) {
              // unstuff remaining
              LOG(("line: push back %ld bytes\n", size - pos));
              sockio_push_back(sio, ptr, size - pos);
            }
          } else {
            // no newline found -> keep all and wait for next char
            got += size;
            LOG(("line: no newline: got %ld bytes\n", got));

            // now wait for more chars
            msg = sockio_wait_char(sio, timeout_us);
            if(msg == NULL) {
              LOG(("line: error waiting!\n"));
              stay = FALSE;
            }
          }
          break;
        }
        case SOCKIO_MSG_ERROR:
          stay = FALSE;
          LOG(("line: sockio ERROR\n"));
          break;
        default:
          LOG(("line: invalid sockio msg!\n"));
          break;
        }
        sockio_free_msg(sio, smsg);
      }
    }
  }

  LOG(("line: read done: ret=%ld\n", result));
  return result;
}

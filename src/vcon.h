#ifndef VCON
#define VCON

#include "buf.h"

#define VCON_MSG_READ           0
#define VCON_MSG_WRITE          1
#define VCON_MSG_WAIT_CHAR      2
#define VCON_MSG_BUFFER_MODE    3
#define VCON_MSG_END            4
#define VCON_MSG_ERROR          5

#define VCON_STATE_OPEN    0
#define VCON_STATE_CLOSE   1
#define VCON_STATE_ERROR   2
#define VCON_STATE_FLUSH   3

#define VCON_MODE_UNSUPPORTED   0xff
#define VCON_MODE_COOKED        0
#define VCON_MODE_RAW           1
#define VCON_MODE_MEDIUM        2

struct vcon_msg
{
  struct Message msg;
  APTR           user_data;
  APTR           private;
  buf_t          buffer;
  UBYTE          type;
  UBYTE          buffer_mode;
};
typedef struct vcon_msg vcon_msg_t;

struct vcon_handle;
typedef struct vcon_handle vcon_handle_t;

/* init console and specify port where to receive vcon_msgs */
vcon_handle_t *vcon_init(struct MsgPort *user_port, ULONG max_msgs);

/* shut down vcon */
void vcon_exit(vcon_handle_t *sh);

/* cleanup all pending vcon_msgs and dos pkts */
void vcon_cleanup(vcon_handle_t *sh);

/* create a new file handle for vcon. has to be closed on your own! */
BPTR  vcon_create_fh(vcon_handle_t *sh);

/* return the sigmask the handle call will respond to */
ULONG vcon_get_sigmask(vcon_handle_t *sh);

/* send a signal mask to the console SIGBREAKF_CTRL_C ... */
BOOL vcon_send_signal(vcon_handle_t *sh, ULONG sig_mask);

/* process the signals the vcon handles. return VCON_STATE_* */
BOOL vcon_handle_sigmask(vcon_handle_t *sh, ULONG sig_mask);

/* flush all pending con requests and EOF reads() */
void vcon_flush(vcon_handle_t *sh);

#endif

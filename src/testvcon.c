#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <clib/alib_protos.h>

#include <string.h>

#include "shell.h"
#include "vcon.h"

#define ZERO 0

/* lib bases */
extern struct ExecBase *SysBase;
struct DosLibrary *DOSBase;

static BOOL handle_vmsg(vcon_msg_t *vmsg)
{
  Printf("test: got VMSG: type=%ld pkt=%lx\n", (LONG)vmsg->type, vmsg->private);
  buf_t *buffer = &vmsg->buffer;
  BOOL stay = TRUE;

  switch(vmsg->type) {
  case VCON_MSG_WRITE:
    Printf("test: WRITE size=%ld\n", buffer->size);
    PutStr("test: VDATA[");
    FWrite(Output(), buffer->data, buffer->size, 1);
    PutStr("]\n");
    break;

  case VCON_MSG_READ:
    Printf("test: READ size=%ld\n", buffer->size);
    LONG actual_size = Read(Input(), buffer->data, buffer->size);
    buffer->size = actual_size;
    Printf("test: READ actual=%ld\n", buffer->size);
    break;

  case VCON_MSG_BUFFER_MODE:
    Printf("test: BUFFER MODE=%ld\n", (LONG)vmsg->buffer_mode);
    break;

  case VCON_MSG_WAIT_CHAR: {
    ULONG timeout_us = buffer->size;
    Printf("test: WAIT CHAR timeout=%ld\n", timeout_us);
    BOOL ok = WaitForChar(Input(), timeout_us);
    if(ok) {
      Printf("test: OK!\n");
      vmsg->buffer.size = 1;
    } else {
      Printf("test: TIMEOUT!\n");
      vmsg->buffer.size = 0;
    }
    break;
    }

  case VCON_MSG_END: {
    PutStr("test: VCON end!\n");
    stay = FALSE;
    break;
  }
  }

  PutStr("test: reply VMSG\n");
  ReplyMsg((struct Message *)vmsg);

  return stay;
}

int testvcon(void)
{
  shell_handle_t *sh;
  vcon_handle_t *sc;
  struct MsgPort *vmsg_port;
  buf_t buffer;

  PutStr("test: Create msg port\n");
  vmsg_port = CreateMsgPort();
  if(vmsg_port == NULL) {
    PutStr("ERROR getting msg port!\n");
    return RETURN_ERROR;
  }

  PutStr("test: Setup shell console\n");
  sc = vcon_init(vmsg_port, 8);
  if(sc == NULL) {
    DeleteMsgPort(vmsg_port);
    PutStr("ERROR setting up shell console...\n");
    return RETURN_ERROR;
  }

  PutStr("test: Setup shell\n");
  BPTR fh = vcon_create_fh(sc);
  sh = shell_init(fh, ZERO, NULL, 8192);
  if(sh == NULL) {
    PutStr("ERROR setting up shell...\n");
    vcon_exit(sc);
    DeleteMsgPort(vmsg_port);
    return RETURN_ERROR;
  }

  PutStr("test: Main loop...\n");
  ULONG shell_mask = shell_exit_mask(sh);
  ULONG con_mask = vcon_get_sigmask(sc);
  ULONG vmsg_mask = 1 << vmsg_port->mp_SigBit;
  ULONG masks = shell_mask | con_mask | vmsg_mask |
    SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F;
  int ctrl_c_count = 0;

  while(TRUE) {
    ULONG got_mask = Wait(masks);

    // handle vcon
    if(got_mask & con_mask) {
      PutStr("test: handle VCON\n");
      BOOL ok = vcon_handle_sigmask(sc, got_mask & con_mask);
      if(ok) {
        PutStr("test: VCON ok\n");
      }
      else {
        PutStr("test: VCON ERROR!!\n");
      }
    }

    // handle incoming vcon_msg_t from vcon
    BOOL stay = TRUE;
    if(got_mask & vmsg_mask) {
      struct Message *msg;
      while((msg = GetMsg(vmsg_port)) != NULL) {
        BOOL my_stay = handle_vmsg((vcon_msg_t *)msg);
        stay = stay && my_stay;
      }
    }

    // shell reports end
    if(got_mask & shell_mask) {
      PutStr("test: shell signal!\n");
    }

    // handle ctrl signals
    if(got_mask & SIGBREAKF_CTRL_C) {
      if(ctrl_c_count == 0) {
        PutStr("test: ----> Ctrl-C signal\n");
        vcon_send_signal(sc, SIGBREAKF_CTRL_C);
      } else {
        PutStr("test: ----> flush vcon\n");
        vcon_flush(sc);
      }
      ctrl_c_count++;
    }
    if(got_mask & SIGBREAKF_CTRL_D) {
      PutStr("test: Ctrl-D signal\n");
      vcon_send_signal(sc, SIGBREAKF_CTRL_D);
    }
    if(got_mask & SIGBREAKF_CTRL_E) {
      PutStr("test: Ctrl-E signal\n");
      vcon_send_signal(sc, SIGBREAKF_CTRL_E);
    }
    if(got_mask & SIGBREAKF_CTRL_F) {
      PutStr("test: Ctrl-F signal\n");
      vcon_send_signal(sc, SIGBREAKF_CTRL_F);
    }

    if(!stay) {
      break;
    }
  }

  PutStr("test: Exit shell\n");
  LONG rc = shell_exit(sh);
  Printf("test: done rc=%ld\n", rc);

  PutStr("test: vcon exit\n");
  vcon_exit(sc);
  DeleteMsgPort(vmsg_port);

  PutStr("test: all done\n");
  return RETURN_OK;
}

int main(void)
{
  int result;

  DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR) "dos.library", 0L);

  result = testvcon();

  CloseLibrary((struct Library *)DOSBase);

  return result;
}

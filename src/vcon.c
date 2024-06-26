#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>

#include <dos/dostags.h>

#define ZERO 0
#define LOG_ENABLED

#include "vcon.h"
#include "log.h"
#include "listutil.h"

#define VCON_MSG_FREE     0xff

struct vcon_handle {
  struct MsgPort          *pkt_port;
  struct MsgPort          *signal_port;
  struct MsgPort          *user_port;
  struct MsgPort          *vmsg_port;
  struct List              free_vmsg_list;
  struct vcon_msg         *vmsgs;
  ULONG                    max_msgs;
  ULONG                    pkt_sigmask;
  ULONG                    vmsg_sigmask;
  ULONG                    open_cnt;
  UBYTE                    buffer_mode;
  UBYTE                    state;
};

static struct DosPacket *msg_to_pkt(struct Message *Msg)
{
  return (struct DosPacket *)Msg->mn_Node.ln_Name;
}

vcon_handle_t *vcon_init(struct MsgPort *user_port, ULONG max_msgs)
{
  /* check param */
  if(max_msgs < 8) {
    max_msgs = 8;
  }
  if(user_port == NULL) {
    return NULL;
  }

  /* create handle */
  vcon_handle_t *sh = (vcon_handle_t *)AllocVec(sizeof(vcon_handle_t), MEMF_ANY | MEMF_CLEAR);
  if(sh == NULL) {
    return NULL;
  }

  /* where to send new vmsgs ... */
  sh->user_port = user_port;
  sh->max_msgs = max_msgs;

  /* alloc buffer for vmsgs */
  ULONG vmsg_size = (sizeof(vcon_msg_t) + 3) & ~3;
  struct vcon_msg *msgs = (struct vcon_msg *)AllocVec(vmsg_size * max_msgs, MEMF_ANY | MEMF_CLEAR);
  if(msgs == NULL) {
    goto fail;
  }
  sh->vmsgs = msgs;

  /* setup con handle */
  NewList(&sh->free_vmsg_list);

  /* fill free list */
  for(ULONG i=0;i<max_msgs;i++) {
    struct vcon_msg *msg = &msgs[i];
    msg->type = VCON_MSG_FREE;
    AddTail(&sh->free_vmsg_list, (struct Node *)msg);
  }

  /* create a msg port for con dos packets */
  sh->pkt_port = CreateMsgPort();
  if(sh->pkt_port == NULL) {
    goto fail;
  }

  /* create a msg port for replied vmsgs from the user */
  sh->vmsg_port = CreateMsgPort();
  if(sh->vmsg_port == NULL) {
    goto fail;
  }

  /* signals for pkt and vmsg port */
  sh->pkt_sigmask = 1 << sh->pkt_port->mp_SigBit;
  sh->vmsg_sigmask = 1 << sh->vmsg_port->mp_SigBit;

  /* no handle open yet */
  sh->state = VCON_STATE_CLOSE;

  /* all ok. return handle */
  return sh;

fail:
  vcon_exit(sh);
  return NULL;
}

void vcon_cleanup(vcon_handle_t *sh)
{
  for(ULONG i=0;i<sh->max_msgs;i++) {
    vcon_msg_t *vmsg = &sh->vmsgs[i];
    if(vmsg->type != VCON_MSG_FREE) {
      struct DosPacket *pkt = (struct DosPacket *)vmsg->private;
      LOG(("vcon: cleanup vmsg: %lx type=%ld pkt=%lx\n", vmsg, (LONG)vmsg->type, pkt));

      LONG res1 = DOSFALSE;
      LONG res2 = ERROR_NO_FREE_STORE;
      switch(vmsg->type) {
      case VCON_MSG_READ:
        res1 = 0;
        res2 = 0;
        break;
      case VCON_MSG_WRITE:
        res1 = 0;
        res2 = 0;
        break;
      }

      LOG(("vcon: reply: pkt=%lx res1=%ld res2=%ld\n", pkt, res1, res2));
      ReplyPkt(pkt, res1, res2);
    }
  }
}

void vcon_exit(vcon_handle_t *sh)
{
  if(sh == NULL) {
    return;
  }

  vcon_cleanup(sh);

  if(sh->vmsg_port != NULL) {
    DeleteMsgPort(sh->vmsg_port);
  }

  if(sh->pkt_port != NULL) {
    DeleteMsgPort(sh->pkt_port);
  }

  if(sh->vmsgs != NULL) {
    FreeVec(sh->vmsgs);
  }

  FreeVec(sh);
}

BPTR vcon_create_fh(vcon_handle_t *sh)
{
  if(sh->state == VCON_STATE_ERROR) {
    return ZERO;
  }

  /* create a fake file handle */
  struct FileHandle *fh = AllocDosObjectTags(DOS_FILEHANDLE,
        ADO_FH_Mode,MODE_OLDFILE,
        TAG_END);
  if(fh == NULL) {
    return ZERO;
  }

  fh->fh_Pos  = -1;
  fh->fh_End  = -1;
  fh->fh_Type = sh->pkt_port;
  fh->fh_Args = (LONG)sh;
  fh->fh_Port = (struct MsgPort *)4; // simply a non-zero value to be interactive

  sh->open_cnt ++;
  sh->state = VCON_STATE_OPEN;

  return MKBADDR(fh);
}

ULONG vcon_get_sigmask(vcon_handle_t *sh)
{
  return sh->pkt_sigmask | sh->vmsg_sigmask;
}

BOOL vcon_send_signal(vcon_handle_t *sh, ULONG sig_mask)
{
  if(sh->signal_port != NULL) {
    struct Task *task = sh->signal_port->mp_SigTask;
    if(task != NULL) {
      Signal(task, sig_mask);
      return TRUE;
    }
  }
  return FALSE;
}

static vcon_msg_t *alloc_vmsg(vcon_handle_t *sh, UBYTE type, APTR data, ULONG size, APTR private)
{
  vcon_msg_t *vmsg = (vcon_msg_t *)RemHead(&sh->free_vmsg_list);
  if(vmsg != NULL) {
    vmsg->msg.mn_ReplyPort = sh->vmsg_port;
    vmsg->type = type;
    vmsg->buffer_mode = sh->buffer_mode;
    vmsg->buffer.data = data;
    vmsg->buffer.size = size;
    vmsg->private = private;

    LOG(("vcon: vmsg alloc: %lx type=%ld pkt=%lx\n", vmsg, (LONG)vmsg->type, vmsg->private));
  }
  return vmsg;
}

static void free_vmsg(vcon_handle_t *sh, vcon_msg_t *vmsg)
{
  LOG(("vcon: vmsg free: %lx type=%ld pkt=%lx\n", vmsg, (LONG)vmsg->type, vmsg->private));

  /* re-add to free list */
  AddTail(&sh->free_vmsg_list, (struct Node *)vmsg);

  vmsg->type = VCON_MSG_FREE;
}

static void handle_dos_pkt(vcon_handle_t *sh, struct Message *msg)
{
  struct DosPacket *pkt = msg_to_pkt(msg);
  struct FileHandle *fh=(struct FileHandle *)BADDR((BPTR)pkt->dp_Arg1);

  LONG res1 = DOSTRUE;
  LONG res2 = 0;
  BOOL do_reply = TRUE;
  BOOL ok = TRUE;
  vcon_msg_t *vmsg = NULL;

  LONG type = pkt->dp_Type;
  switch(type) {
    case ACTION_FINDINPUT:
    case ACTION_FINDOUTPUT:
    case ACTION_FINDUPDATE:
      sh->open_cnt++;
      LOG(("vcon: OPEN: fh=%lx name=%b count=%ld\n", fh, BADDR((BPTR)pkt->dp_Arg3), (ULONG)sh->open_cnt));
      fh->fh_Pos = -1;
      fh->fh_End = -1;
      fh->fh_Type= sh->pkt_port;
      fh->fh_Args= (LONG)sh;
      fh->fh_Port= (struct MsgPort *)4;
      break;
    case ACTION_END:
      sh->open_cnt--;
      if(sh->open_cnt == 0) {
        sh->state = VCON_STATE_CLOSE;
      }
      LOG(("vcon: CLOSE: fh=%lx count=%ld\n", fh, (ULONG)sh->open_cnt));
      break;
    case ACTION_CHANGE_SIGNAL: {
      struct MsgPort *msg_port = (struct MsgPort *)pkt->dp_Arg2;
      LOG(("vcon: CHANGE_SIGNAL: fh=%lx msg_port=%lx\n", fh, msg_port));
      /* return old port */
      res1 = DOSTRUE;
      res2 = (LONG)sh->signal_port;
      sh->signal_port = msg_port;
      break;
    }
    case ACTION_SCREEN_MODE: {
      LONG mode = pkt->dp_Arg1;
      LOG(("vcon: SCREEN_MODE: mode=%ld\n", mode));
      sh->buffer_mode = mode;
      /* send mode vmsg */
      vmsg = alloc_vmsg(sh, VCON_MSG_BUFFER_MODE, NULL, 0, pkt);
      if(vmsg != NULL) {
        PutMsg(sh->user_port, (struct Message *)vmsg);
        do_reply = FALSE;
      } else {
        res1 = DOSFALSE;
        res2 = ERROR_NO_FREE_STORE;
      }
      break;
    }
    case ACTION_DISK_INFO: {
      LOG(("vcon: DISK_INFO\n"));
      struct InfoData *id = (struct InfoData *)BADDR((BPTR)pkt->dp_Arg1);
      if(id != NULL) {
        LOG(("id_DiskType %lx\n", id->id_DiskType));
      }
      ULONG mode;
      if(sh->buffer_mode == 0) { // cooked
        id->id_DiskType = 0x434f4e00; // CON\0
      }
      else {
        id->id_DiskType = 0x52415700; // RAW\0
      }
      id->id_VolumeNode = 0; // no intuition window
      id->id_InUse = 0; // no IOReq
      break;
    }
    case ACTION_WRITE: {
      APTR buf = (APTR)pkt->dp_Arg2;
      LONG size = pkt->dp_Arg3;
      LOG(("vcon: WRITE: pkt=%lx fh=%lx buf=%lx size=%ld\n", pkt, fh, buf, size));
      /* send write vmsg */
      vmsg = alloc_vmsg(sh, VCON_MSG_WRITE, buf, size, pkt);
      if(vmsg != NULL) {
        PutMsg(sh->user_port, (struct Message *)vmsg);
        do_reply = FALSE;
      } else {
        res1 = DOSFALSE;
        res2 = ERROR_NO_FREE_STORE;
        sh->state = VCON_STATE_ERROR;
      }
      break;
    }
    case ACTION_READ: {
      APTR buf = (APTR)pkt->dp_Arg2;
      LONG size = pkt->dp_Arg3;
      LOG(("vcon: READ: pkt=%lx fh=%lx buf=%lx size=%ld\n", pkt, fh, buf, size));
      /* send read vmsg */
      vmsg = alloc_vmsg(sh, VCON_MSG_READ, buf, size, pkt);
      if(vmsg != NULL) {
        PutMsg(sh->user_port, (struct Message *)vmsg);
        do_reply = FALSE;
      } else {
        res1 = DOSFALSE;
        res2 = ERROR_NO_FREE_STORE;
        sh->state = VCON_STATE_ERROR;
      }
      /* implicitly set signal port to reader */
      sh->signal_port = pkt->dp_Port;
      break;
    }
    case ACTION_SEEK: {
      LONG offset = pkt->dp_Arg2;
      LONG mode = pkt->dp_Arg3;
      LOG(("vcon: SEEK: offset=%ld mode=%ld\n", offset, mode));
      res1 = DOSFALSE;
      res2 = ERROR_OBJECT_WRONG_TYPE;
      break;
    }
    case ACTION_WAIT_CHAR: {
      ULONG time_us = pkt->dp_Arg1;
      LOG(("vcon: WAIT_CHAR: pkt=%lx time us=%lu\n", pkt, time_us));
      /* send wait char vmsg */
      vmsg = alloc_vmsg(sh, VCON_MSG_WAIT_CHAR, NULL, 0, pkt);
      if(vmsg != NULL) {
        /* store timeout in buffer.size */
        vmsg->buffer.size = time_us;
        /* send msg */
        PutMsg(sh->user_port, (struct Message *)vmsg);
        do_reply = FALSE;
      } else {
        res1 = DOSFALSE;
        res2 = ERROR_NO_FREE_STORE;
        sh->state = VCON_STATE_ERROR;
      }
      break;
    }
    default:
      LOG(("vcon: UNKNOWN PKT: %ld\n", type));
      res1 = DOSFALSE;
      res2 = ERROR_ACTION_NOT_KNOWN;
      break;
  }
  if(do_reply) {
      LOG(("vcon: reply: pkt=%lx res1=%ld res2=%ld\n", pkt, res1, res2));
      ReplyPkt(pkt, res1, res2);
  }
}

static void handle_vmsg_reply(vcon_handle_t *sh, struct Message *msg)
{
  vcon_msg_t *vmsg = (vcon_msg_t *)msg;
  struct DosPacket *pkt = (struct DosPacket *)vmsg->private;
  LOG(("vcon: reply: vmsg: %lx type=%ld pkt=%lx\n", vmsg, (LONG)vmsg->type, pkt));

  LONG res1 = DOSTRUE;
  LONG res2 = 0;
  switch(vmsg->type) {
  case VCON_MSG_READ:
  case VCON_MSG_WRITE:
    res1 = vmsg->buffer.size;
    break;
  case VCON_MSG_WAIT_CHAR:
    /* extract number of lines from buffer.size */
    if(vmsg->buffer.size == 0) {
      /* no char received */
      res1 = DOSFALSE;
      res2 = 0;
    } else {
      /* char available return number of lines */
      res1 = DOSTRUE;
      res2 = vmsg->buffer.size;
    }
    break;
  }

  if(pkt != NULL) {
    LOG(("vcon: reply: pkt=%lx res1=%ld res2=%ld\n", pkt, res1, res2));
    ReplyPkt(pkt, res1, res2);
  }

  free_vmsg(sh, vmsg);
}

ULONG vcon_handle_sigmask(vcon_handle_t *sh, ULONG got_mask)
{
  ULONG flags = 0;

  /* handle DOS packets for our virtual console */
  if(got_mask & sh->pkt_sigmask) {
    struct Message *msg;
    while((msg = GetMsg(sh->pkt_port)) != NULL) {
      handle_dos_pkt(sh, msg);
    }
  }

  /* handle replied vcon messages from user */
  if(got_mask & sh->vmsg_sigmask) {
    struct Message *msg;
    while((msg = GetMsg(sh->vmsg_port)) != NULL) {
      handle_vmsg_reply(sh, msg);
    }
  }

  return sh->state;
}

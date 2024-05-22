#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>

#include <dos/dostags.h>

#define ZERO 0

#include "vcon.h"
#include "log.h"

#define GetHead(list) (((list) && (list)->lh_Head && (list)->lh_Head->ln_Succ) \
        ? (list)->lh_Head : (struct Node *)NULL)
#define GetSucc(node) (((node) && (node)->ln_Succ && (node)->ln_Succ->ln_Succ) \
        ? (node)->ln_Succ : (struct Node *)NULL)

struct vcon_handle {
  struct MsgPort          *msg_port;
  struct MsgPort          *signal_port;
  struct List             rw_list;
  struct DosPacket       *head_pkt;
  ULONG                   sigmask_port;
  ULONG                   open_cnt;
  LONG                    buffer_mode;
  LONG                    cur_flags;
};

static struct DosPacket *msg_to_pkt(struct Message *Msg)
{
  return (struct DosPacket *)Msg->mn_Node.ln_Name;
}

vcon_handle_t *vcon_init(void)
{
  /* create handle */
  vcon_handle_t *sh = (vcon_handle_t *)AllocVec(sizeof(vcon_handle_t), MEMF_ANY | MEMF_CLEAR);
  if(sh == NULL) {
    return NULL;
  }

  /* create a msg port for con dos packets */
  struct MsgPort *msg_port = CreateMsgPort();
  if(msg_port == NULL) {
    FreeVec(sh);
    return NULL;
  }


  /* setup con handle */
  NewList(&sh->rw_list);
  sh->msg_port = msg_port;
  sh->sigmask_port = 1 << sh->msg_port->mp_SigBit;
  sh->open_cnt = 1;
  sh->signal_port = NULL;
  sh->buffer_mode = 0;
  sh->cur_flags = 0;

  return sh;
}

void vcon_exit(vcon_handle_t *sh)
{
  if(sh->msg_port != NULL) {
    DeleteMsgPort(sh->msg_port);
  }

  FreeVec(sh);
}

BPTR vcon_create_fh(vcon_handle_t *sh)
{
  /* create a fake file handle */
  struct FileHandle *fh = AllocDosObjectTags(DOS_FILEHANDLE,
        ADO_FH_Mode,MODE_OLDFILE,
        TAG_END);
  if(fh == NULL) {
    return ZERO;
  }

  fh->fh_Pos  = -1;
  fh->fh_End  = -1;
  fh->fh_Type = sh->msg_port;
  fh->fh_Args = (LONG)sh;
  fh->fh_Port = (struct MsgPort *)4; // simply a non-zero value to be interactive

  return MKBADDR(fh);
}

LONG vcon_get_buffer_mode(vcon_handle_t *sh)
{
  return sh->buffer_mode;
}

ULONG vcon_get_sigmask(vcon_handle_t *sh)
{
  return sh->sigmask_port;
}

static ULONG handle_pkt(vcon_handle_t *sh, struct Message *msg)
{
  ULONG flags = 0;
  struct DosPacket *pkt = msg_to_pkt(msg);
  struct FileHandle *fh=(struct FileHandle *)BADDR((BPTR)pkt->dp_Arg1);

  LONG res1 = DOSTRUE;
  LONG res2 = 0;
  BOOL do_reply = TRUE;

  LONG type = pkt->dp_Type;
  switch(type) {
    case ACTION_FINDINPUT:
    case ACTION_FINDOUTPUT:
    case ACTION_FINDUPDATE:
      sh->open_cnt++;
      LOG(("OPEN: fh=%lx name=%b count=%ld\n", fh, BADDR((BPTR)pkt->dp_Arg3), (ULONG)sh->open_cnt));
      fh->fh_Pos = -1;
      fh->fh_End = -1;
      fh->fh_Type= sh->msg_port;
      fh->fh_Args= (LONG)sh;
      fh->fh_Port= (struct MsgPort *)4;
      break;
    case ACTION_END:
      sh->open_cnt--;
      LOG(("CLOSE: fh=%lx count=%ld\n", fh, (ULONG)sh->open_cnt));
      if(sh->open_cnt == 0) {
        flags = VCON_HANDLE_CLOSE;
      }
      break;
    case ACTION_CHANGE_SIGNAL: {
      struct MsgPort *msg_port = (struct MsgPort *)pkt->dp_Arg2;
      LOG(("CHANGE_SIGNAL: fh=%lx msg_port=%lx\n", fh, msg_port));
      /* return old port */
      res1 = DOSTRUE;
      res2 = (LONG)sh->signal_port;
      sh->signal_port = msg_port;
      break;
    }
    case ACTION_SCREEN_MODE: {
      LONG mode = pkt->dp_Arg1;
      LOG(("SCREEN_MODE: mode=%ld\n", mode));
      sh->buffer_mode = mode;
      flags = VCON_HANDLE_MODE;
      break;
    }
    case ACTION_DISK_INFO: {
      LOG(("DISK_INFO\n"));
      break;
    }
    case ACTION_WRITE: {
      APTR buf = (APTR)pkt->dp_Arg2;
      LONG size = pkt->dp_Arg3;
      LOG(("WRITE: fh=%lx buf=%lx size=%ld %s\n", fh, buf, size, buf));
      /* add to rw list and do not reply now */
      do_reply = FALSE;
      AddTail(&sh->rw_list, (struct Node *)msg);
      break;
    }
    case ACTION_READ: {
      APTR buf = (APTR)pkt->dp_Arg2;
      LONG size = pkt->dp_Arg3;
      LOG(("READ: fh=%lx buf=%lx size=%ld\n", fh, buf, size));
      /* add to rw list and do not reply now */
      do_reply = FALSE;
      AddTail(&sh->rw_list, (struct Node *)msg);
      /* implicitly set signal port to reader */
      sh->signal_port = pkt->dp_Port;
      break;
    }
    case ACTION_SEEK: {
      LONG offset = pkt->dp_Arg2;
      LONG mode = pkt->dp_Arg3;
      LOG(("SEEK: offset=%ld mode=%ld\n", offset, mode));
      res1 = DOSFALSE;
      res2 = ERROR_OBJECT_WRONG_TYPE;;
      break;
    }
    default:
      LOG(("Unknown PKT: %ld\n", type));
      res1 = DOSFALSE;
      res2 = ERROR_ACTION_NOT_KNOWN;
      break;
  }
  if(do_reply) {
      LOG(("do-reply: res1=%ld res2=%ld\n", res1, res2));
      ReplyPkt(pkt, res1, res2);
  }
  return flags;
}

static ULONG update_rw_flags(vcon_handle_t *sh)
{
  ULONG flags = 0;

  /* what's next on rw list? */
  sh->head_pkt = NULL;
  struct Message *msg = (struct Message *)GetHead(&sh->rw_list);
  if(msg != NULL) {
    struct DosPacket *pkt = msg_to_pkt(msg);
    /* update head packet */
    sh->head_pkt = pkt;

    LONG type = pkt->dp_Type;
    LOG(("Head packet type: %ld\n", type));
    if(type == ACTION_READ) {
      flags = VCON_HANDLE_READ;
    }
    else if(type == ACTION_WRITE) {
      flags = VCON_HANDLE_WRITE;
    }
  } else {
    LOG(("No head packet!\n"));
  }

  return flags;
}

BOOL vcon_signal(vcon_handle_t *sh, ULONG sig_mask)
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

ULONG vcon_handle(vcon_handle_t *sh)
{
  ULONG flags = 0;

  /* handle DOS packets for our con */
  struct Message *msg;
  while((msg = GetMsg(sh->msg_port)) != NULL) {
    flags |= handle_pkt(sh, msg);
  }

  /* update the rw flags */
  flags |= update_rw_flags(sh);

  /* keep current flags for follow up commands */
  sh->cur_flags = flags;
  return flags;
}

LONG vcon_read_begin(vcon_handle_t *sh, APTR *ret_buf)
{
  /* no read pending? */
  if((sh->cur_flags & VCON_HANDLE_READ) == 0) {
    return -1;
  }
  if(sh->head_pkt == NULL) {
    return -1;
  }

  struct DosPacket *pkt = sh->head_pkt;
  APTR buf = (APTR)pkt->dp_Arg2;
  LONG size = pkt->dp_Arg3;

  *ret_buf = buf;
  return size;
}

void vcon_read_end(vcon_handle_t *sh, LONG size)
{
  /* remove from rw list */
  RemHead(&sh->rw_list);

  /* reply dos packet */
  ReplyPkt(sh->head_pkt, size, 0);

  sh->head_pkt = NULL;
}

LONG vcon_write_begin(vcon_handle_t *sh, APTR *ret_buf)
{
  /* no write pending? */
  if((sh->cur_flags & VCON_HANDLE_WRITE) == 0) {
    return -1;
  }
  if(sh->head_pkt == NULL) {
    return -1;
  }

  struct DosPacket *pkt = sh->head_pkt;
  APTR buf = (APTR)pkt->dp_Arg2;
  LONG size = pkt->dp_Arg3;

  *ret_buf = buf;
  return size;
}

void vcon_write_end(vcon_handle_t *sh, LONG size)
{
  /* remove from rw list */
  RemHead(&sh->rw_list);

  /* reply dos packet */
  ReplyPkt(sh->head_pkt, size, 0);

  sh->head_pkt = NULL;
}

void vcon_drop(vcon_handle_t *sh)
{
  struct Node *node = GetHead(&sh->rw_list);
  while(node != NULL) {
    /* abort all packets */
    struct Message *msg = (struct Message *)node;
    struct DosPacket *pkt = msg_to_pkt(msg);
    LONG type = pkt->dp_Type;
    LONG result;
    if(type == ACTION_READ) {
      /* read report EOF */
      result = 0;
    }
    else {
      /* pretend to write all */
      result = pkt->dp_Arg3;
    }

    ReplyPkt(pkt, result, 0);

    node = GetSucc(node);
  }
  /* clear list */
  NewList(&sh->rw_list);
}


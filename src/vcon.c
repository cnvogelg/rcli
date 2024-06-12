#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>

#include <dos/dostags.h>

#define ZERO 0
//#define LOG_ENABLED

#include "vcon.h"
#include "log.h"
#include "listutil.h"
#include "timer.h"

struct vcon_handle {
  struct MsgPort          *msg_port;
  struct MsgPort          *signal_port;
  timer_handle_t         *timer;
  struct List             rw_list;
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
  /* setup timer */
  timer_handle_t *timer = timer_init();
  if(timer == NULL) {
    return NULL;
  }

  /* create handle */
  vcon_handle_t *sh = (vcon_handle_t *)AllocVec(sizeof(vcon_handle_t), MEMF_ANY | MEMF_CLEAR);
  if(sh == NULL) {
    return NULL;
  }

  sh->timer = timer;

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
  if(sh == NULL) {
    return;
  }

  if(sh->msg_port != NULL) {
    DeleteMsgPort(sh->msg_port);
  }

  if(sh->timer != NULL) {
    timer_exit(sh->timer);
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
  return sh->sigmask_port | timer_get_sig_mask(sh->timer);
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
      LOG(("WRITE: pkt=%lx fh=%lx buf=%lx size=%ld\n", pkt, fh, buf, size));
      /* add to rw list and do not reply now */
      do_reply = FALSE;
      AddTail(&sh->rw_list, (struct Node *)msg);
      break;
    }
    case ACTION_READ: {
      APTR buf = (APTR)pkt->dp_Arg2;
      LONG size = pkt->dp_Arg3;
      LOG(("READ: pkt=%lx fh=%lx buf=%lx size=%ld\n", pkt, fh, buf, size));
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
      res2 = ERROR_OBJECT_WRONG_TYPE;
      break;
    }
    case ACTION_WAIT_CHAR: {
      ULONG time_us = pkt->dp_Arg1;
      LOG(("WAIT_CHAR: pkt=%lx time us=%lu\n", pkt, time_us));
      ULONG time_s = 0;
      if(time_us > 1000000UL) {
        time_s = time_us / 1000000UL;
        time_us %= 1000000UL;
      }
      LOG(("WAIT_CHAR: time s=%lu us=%lu\n", time_s, time_us));
      timer_job_t *job = timer_start(sh->timer, time_s, time_us, pkt);
      do_reply = FALSE;
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

static struct DosPacket *get_first_rw_pkt(vcon_handle_t *sh)
{
  /* what's next on rw list? */
  struct Message *msg = (struct Message *)GetHead(&sh->rw_list);
  if(msg != NULL) {
    struct DosPacket *pkt = msg_to_pkt(msg);
    return pkt;
  } else {
    return NULL;
  }
}

static ULONG update_rw_flags(vcon_handle_t *sh)
{
  ULONG flags = 0;

  /* what's next on rw list? */
  struct DosPacket *pkt = get_first_rw_pkt(sh);
  if(pkt != NULL) {
    LONG type = pkt->dp_Type;
    LOG(("update rw: Head packet type: %ld\n", type));
    if(type == ACTION_READ) {
      flags = VCON_HANDLE_READ;
    }
    else if(type == ACTION_WRITE) {
      flags = VCON_HANDLE_WRITE;
    }
  } else {
    LOG(("update rw: No head packet!\n"));
  }

  return flags;
}

static ULONG update_wait_char_flag(vcon_handle_t *sh)
{
  timer_job_t *job = timer_get_first_job(sh->timer);
  if(job != NULL) {
    return VCON_HANDLE_WAIT_CHAR;
  }

  return 0;
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

ULONG vcon_handle_sigmask(vcon_handle_t *sh, ULONG got_mask)
{
  ULONG flags = 0;

  /* handle DOS packets for our con */
  if(got_mask & sh->sigmask_port) {
    struct Message *msg;
    while((msg = GetMsg(sh->msg_port)) != NULL) {
      flags |= handle_pkt(sh, msg);
    }
  }

  /* handle timeout jobs from wait char */
  ULONG timer_mask = timer_get_sig_mask(sh->timer);
  if(got_mask & timer_mask) {
    while(TRUE) {
      timer_job_t *job = timer_get_next_done_job(sh->timer);
      if(job == NULL) {
        break;
      }
      /* get associated dos packet */
      struct DosPacket *pkt = (struct DosPacket *)timer_job_get_user_data(job);
      /* report no key */
      ReplyPkt(pkt, DOSFALSE, 0);
      LOG(("WAIT_CHAR: timeout pkt=%lx\n", pkt));

      timer_stop(job);
    }
  }

  /* update the rw flags */
  flags |= update_rw_flags(sh);
  flags |= update_wait_char_flag(sh);

  /* keep current flags for follow up commands */
  sh->cur_flags = flags;
  return flags;
}

BOOL vcon_read_begin(vcon_handle_t *sh, vcon_buf_t *buf)
{
  /* no read pending? */
  if((sh->cur_flags & VCON_HANDLE_READ) == 0) {
    return FALSE;
  }

  struct DosPacket *pkt = get_first_rw_pkt(sh);
  if(pkt == NULL) {
    return FALSE;
  }

  buf->data = (APTR)pkt->dp_Arg2;
  buf->size = pkt->dp_Arg3;
  buf->private = pkt;

  return TRUE;
}

void vcon_read_end(vcon_handle_t *sh, vcon_buf_t *buf)
{
  /* remove from rw list */
  RemHead(&sh->rw_list);

  struct DosPacket *pkt = (struct DosPacket *)buf->private;

  /* reply dos packet */
  ReplyPkt(pkt, buf->size, 0);
}

BOOL vcon_write_begin(vcon_handle_t *sh, vcon_buf_t *buf)
{
  /* no write pending? */
  if((sh->cur_flags & VCON_HANDLE_WRITE) == 0) {
    return FALSE;
  }

  struct DosPacket *pkt = get_first_rw_pkt(sh);
  if(pkt == NULL) {
    return FALSE;
  }

  buf->data = (APTR)pkt->dp_Arg2;
  buf->size = pkt->dp_Arg3;
  buf->private = pkt;

  return TRUE;
}

void vcon_write_end(vcon_handle_t *sh, vcon_buf_t *buf)
{
  /* remove from rw list */
  RemHead(&sh->rw_list);

  struct DosPacket *pkt = (struct DosPacket *)buf->private;

  /* reply dos packet */
  ReplyPkt(pkt, buf->size, 0);
}

static void drop_rw_pkts(vcon_handle_t *sh)
{
  struct Node *node = RemHead(&sh->rw_list);
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

    LOG(("DROP: rw pkt %lx\n", pkt));
    ReplyPkt(pkt, result, 0);

    node = RemHead(&sh->rw_list);
  }
  /* clear list */
  NewList(&sh->rw_list);
}

static void drop_waitchar_pkts(vcon_handle_t *sh)
{
  timer_job_t *job = timer_get_first_job(sh->timer);
  while(job != NULL) {
    LOG(("DROP: timer job for pkt %lx\n"));
    timer_job_t *next_job = timer_get_next_job(job);

    /* get dos packet from job */
    struct DosPacket *pkt = (struct DosPacket *)timer_job_get_user_data(job);
    /* reply timeout */
    ReplyPkt(pkt, DOSFALSE, 0);

    timer_stop(job);

    Remove((struct Node *)job);

    job = next_job;
  }
}

void vcon_drop(vcon_handle_t *sh)
{
  drop_rw_pkts(sh);
  drop_waitchar_pkts(sh);
}

BOOL vcon_waitchar_get_wait_time(vcon_handle_t *sh, ULONG *wait_s, ULONG *wait_us)
{
  timer_job_t *job = timer_get_first_job(sh->timer);
  if(job == NULL) {
    return FALSE;
  }

  timer_job_get_wait_time(job, wait_s, wait_us);
  return TRUE;
}

/* report a waiting key */
BOOL vcon_waitchar_report(vcon_handle_t *sh)
{
  timer_job_t *job = timer_get_first_job(sh->timer);
  if(job == NULL) {
    return FALSE;
  }

  /* get dos packet from job */
  struct DosPacket *pkt = (struct DosPacket *)timer_job_get_user_data(job);
  /* reply success */
  ReplyPkt(pkt, DOSTRUE, 0);
  LOG(("WAIT_CHAR: OK pkt=%lx\n", pkt));

  timer_stop(job);

  return TRUE;
}


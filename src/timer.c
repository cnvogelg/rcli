#include <proto/exec.h>
#include <proto/timer.h>
#include <devices/timer.h>

#include <clib/alib_protos.h>

#include "compiler.h"
#include "timer.h"
#include "listutil.h"

struct timer_handle
{
  struct MsgPort *port;
  struct List job_list;
  struct Device *device;
  struct Unit *unit;
  UWORD  num_jobs;
};

struct timer_job
{
  struct MinNode node;
  struct timerequest req;
  APTR user_data;
  ULONG wait_s;
  ULONG wait_us;
  struct timer_handle *handle;
};

timer_handle_t *timer_init(void)
{
  /* alloc handle */
  struct timer_handle *th;
  th = AllocVec(sizeof(struct timer_handle), MEMF_CLEAR | MEMF_ANY);
  if (th == NULL)
  {
    return NULL;
  }

  /* create msg port for timer access */
  th->port = CreateMsgPort();
  if (th->port == NULL)
  {
    FreeVec(th);
    return NULL;
  }

  NewList(&th->job_list);

  return th;
}

void timer_exit(timer_handle_t *th)
{
  if (th == NULL)
  {
    return;
  }

  /* cleanup pending jobs */
  struct Node *node = GetHead(&th->job_list);
  while(node != NULL) {
    struct Node *next_node = GetSucc(node);
    timer_stop((timer_job_t *)node);
    node = next_node;
  }

  if(th->port != NULL) {
    DeleteMsgPort(th->port);
  }

  FreeVec(th);
}

ULONG timer_get_sig_mask(timer_handle_t *th)
{
  return 1 << th->port->mp_SigBit;
}

timer_job_t *timer_get_next_done_job(timer_handle_t *th)
{
  struct Message *msg = GetMsg(th->port);
  if(msg == NULL) {
    return NULL;
  }
  /* same trick as in DOS packet: use name pointer */
  timer_job_t *job = (timer_job_t *)msg->mn_Node.ln_Name;
  return job;
}

BOOL timer_has_jobs(timer_handle_t *th)
{
  struct Node *node = GetHead(&th->job_list);
  return node != NULL;
}

timer_job_t *timer_get_first_job(timer_handle_t *th)
{
  struct Node *node = GetHead(&th->job_list);
  return (timer_job_t *)node;
}

timer_job_t *timer_get_next_job(timer_job_t *job)
{
  struct Node *node = GetSucc((struct Node *)job);
  return (timer_job_t *)node;
}

timer_job_t *timer_start(timer_handle_t *th, ULONG secs, ULONG micros, APTR user_data)
{
  timer_job_t *job = AllocVec(sizeof(timer_job_t), MEMF_CLEAR | MEMF_ANY);
  if(job == NULL) {
    return NULL;
  }

  job->handle = th;

  /* keep ref to job */
  struct IORequest *req = (struct IORequest *)&job->req;
  req->io_Message.mn_Node.ln_Name = (APTR)job;

  /* need to open the device */
  if(th->num_jobs == 0) {
    if(OpenDevice((STRPTR)TIMERNAME, UNIT_MICROHZ, req, 0))
    {
      FreeVec(job);
      return NULL;
    }
    th->device = req->io_Device;
    th->unit = req->io_Unit;
  } else {
    req->io_Device = th->device;
    req->io_Unit = th->unit;
  }

  job->req.tr_node.io_Command = TR_ADDREQUEST;
  job->req.tr_node.io_Flags = 0;
  job->req.tr_node.io_Message.mn_ReplyPort = th->port;
  job->req.tr_time.tv_secs = secs;
  job->req.tr_time.tv_micro = micros;
  th->num_jobs++;

  job->wait_s = secs;
  job->wait_us = micros;
  job->user_data = user_data;

  SendIO(req);
  AddTail(&th->job_list, (struct Node *)job);

  return job;
}

void timer_stop(timer_job_t *job)
{
  if(job == NULL) {
    return;
  }

  struct timer_handle *th = job->handle;
  struct IORequest *req = (struct IORequest *)&job->req;
  if (!CheckIO(req))
  {
    AbortIO(req);
  }
  WaitIO(req);

  th->num_jobs--;

  if(th->num_jobs == 0) {
    CloseDevice(req);
    th->device = NULL;
    th->unit = NULL;
  }

  Remove((struct Node *)job);

  FreeVec(job);
}

APTR timer_job_get_user_data(timer_job_t *job)
{
  if(job == NULL) {
    return NULL;
  }

  return job->user_data;
}

void timer_job_get_wait_time(timer_job_t *job, ULONG *wait_s, ULONG *wait_us)
{
  if(job == NULL) {
    return;
  }

  *wait_s = job->wait_s;
  *wait_us = job->wait_us;
}

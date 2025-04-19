#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <clib/alib_protos.h>

#include "timer.h"

/* lib bases */
extern struct ExecBase *SysBase;
struct DosLibrary *DOSBase;

int testtimer(void)
{
  timer_handle_t *th;
  timer_job_t *job;

  PutStr("Setup timer\n");
  th = timer_init();
  if(th == NULL) {
    PutStr("ERROR setting up timer...\n");
    return RETURN_ERROR;
  }

  PutStr("start job\n");
  job = timer_start(th, 5, 0, th);
  if(job == NULL) {
    PutStr("ERROR setting job...\n");
    timer_exit(th);
    return RETURN_ERROR;
  }

  PutStr("Waiting for timer...\n");
  ULONG timer_mask = timer_get_sig_mask(th);
  ULONG wait_mask = timer_mask | SIGBREAKF_CTRL_C;
  ULONG got_mask = Wait(wait_mask);
  if(got_mask & timer_mask) {
    PutStr("Timer triggered!\n");
    timer_job_t *next_job = timer_get_next_done_job(th);
    if(next_job != job) {
      PutStr("wrong job?!\n");
    }
  }
  if(got_mask & SIGBREAKF_CTRL_C) {
    PutStr("Break.\n");
  }

  PutStr("stop job\n");
  timer_stop(job);

  PutStr("Exit timer\n");
  timer_exit(th);
  PutStr("done.\n");

  return RETURN_OK;
}

int main(void)
{
  int result;

  DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR) "dos.library", 0L);

  result = testtimer();

  CloseLibrary((struct Library *)DOSBase);

  return result;
}

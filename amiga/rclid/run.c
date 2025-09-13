#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>

#include <dos/dostags.h>

#ifdef DEBUG_RUN
#define DEBUG
#endif

#include "compiler.h"
#include "run.h"

#define ZERO 0


struct run_handle
{
  struct Task       *call_proc;
  struct Process    *run_proc;
  struct DosLibrary *dos_base; // for start proc
  const char        *run_cmd;
  int               mode;
  BPTR              fh_in;
  BPTR              fh_out;
  BYTE              exit_status;
  BYTE              exit_signal;
};

#define DOSBase run->dos_base
static ASM SAVEDS int start_proc(REG(a0, char *arg_str), REG(d0, long arg_len))
{
  // retrieve my handle via the exit data of the process
  struct Process *me = (struct Process *)FindTask(NULL);
  run_handle_t *run = (run_handle_t *)me->pr_ExitData;

  // open dos for new proc
  DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR)DOSNAME, 0L);
  if(DOSBase == NULL) {
    // set error and notify caller
    run->exit_status = RUN_EXIT_STATUS_NO_DOS;
    Signal(run->call_proc, 1 << run->exit_signal);
    return 1;
  }

  Printf("RUN RUN %s\n", run->run_cmd);

  // run our command
  BOOL ok;
  if(run->mode == RUN_MODE_SHELL) {
    LONG result = Execute(run->run_cmd, run->fh_in, run->fh_out);
    ok = (result == DOSTRUE);
  } else {
    LONG result = SystemTags(run->run_cmd,
      SYS_Input, run->fh_in,
      SYS_Output, run->fh_out,
      TAG_DONE);
    ok = (result != -1);
  }

  Printf("RUN DONE=%ld\n", ok);

  // close streams (and signal vcon)
  Close(run->fh_in);
  Close(run->fh_out);

  // if the shell could not launch then report the exit status from here
  if(ok) {
    run->exit_status = RUN_EXIT_STATUS_OK;
  } else {
    run->exit_status = RUN_EXIT_STATUS_NO_RUN;
  }
  Signal(run->call_proc, 1 << run->exit_signal);

  CloseLibrary((struct Library *)DOSBase);

  return 0;
}
#undef DOSBase

run_handle_t *run_init(int mode, BPTR fh_in, BPTR fh_out, const char *run_cmd,
  ULONG stack_size)
{
  // setup handle
  run_handle_t *run = (run_handle_t *)AllocVec(sizeof(run_handle_t), MEMF_ANY);
  if(run == NULL) {
    return NULL;
  }

  run->mode = mode;

  // default startup
  if((run_cmd == NULL) || (run_cmd[0] == 0)) {
    if(mode == RUN_MODE_SHELL) {
      run_cmd = "Execute S:remote-startup";
    } else {
      run_cmd = "Echo \"No command given!\"";
    }
  }
  run->run_cmd = run_cmd;

  // store caller
  run->call_proc = FindTask(NULL);

  // create start_signal
  run->exit_signal = AllocSignal(-1);
  if(run->exit_signal == -1) {
    FreeVec(run);
    return NULL;
  }

  run->fh_in = fh_in;
  run->fh_out = fh_out;
  run->exit_status = RUN_EXIT_STATUS_INVALID;

  // stdout on console - for debugging
  BPTR out = Open("*", MODE_OLDFILE);

  // create a sub process that will run our command
  CreateNewProcTags(
      NP_Entry, start_proc,
      NP_ExitData, run, // we pass our handle via exit data
      NP_Output, out,
      NP_WindowPtr, -1, // disable reqs from DOS
      TAG_END);

  return run;
}

ULONG run_exit_mask(run_handle_t *run)
{
  return 1 << run->exit_signal;
}

BYTE run_exit(run_handle_t *run)
{
  // blocking wait for end of shell
  if(run->exit_status == RUN_EXIT_STATUS_INVALID) {
    ULONG mask = run_exit_mask(run);
    Wait(mask);
  }

  BYTE result = run->exit_status;

  FreeSignal(run->exit_signal);
  FreeVec(run);

  // return shell result
  return result;
}

#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>

#include <dos/dostags.h>

#ifdef DEBUG_SHELL
#define DEBUG
#endif

#include "compiler.h"
#include "shell.h"

#define ZERO 0


struct shell_handle
{
  struct Task       *call_proc;
  struct Process    *shell_proc;
  struct DosLibrary *dos_base; // for start proc
  const char        *shell_startup;
  BPTR              fh_in;
  BPTR              fh_out;
  BYTE              exit_status;
  BYTE              exit_signal;
};

static ASM SAVEDS int exit_func(REG(d0, LONG rc), REG(d1, LONG exit_data))
{
  shell_handle_t *sh = (shell_handle_t *)exit_data;

  // signal end of shell to caller
  sh->exit_status = SHELL_EXIT_STATUS_OK;
  Signal(sh->call_proc, 1 << sh->exit_signal);

  return rc;
}

#define DOSBase sh->dos_base
static ASM SAVEDS int start_proc(REG(a0, char *arg_str), REG(d0, long arg_len))
{
  // retrieve my handle via the exit data of the process
  struct Process *me = (struct Process *)FindTask(NULL);
  shell_handle_t *sh = (shell_handle_t *)me->pr_ExitData;

  // open dos for new proc
  DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR)DOSNAME, 0L);
  if(DOSBase == NULL) {
    // set error and notify caller
    sh->exit_status = SHELL_EXIT_STATUS_NO_DOS;
    Signal(sh->call_proc, 1 << sh->exit_signal);
    return 1;
  }

  // actually here we would run a synchronous interactive shell.
  // But unfortunately up to AmigaDOS 47 this is not possible.
  // Therefore we have to run an async shell and pass our handle.
  // The NP_ExitCode of the real shell process will then signal the
  // termination of the shell...
  LONG result = SystemTags(NULL,
    SYS_Asynch, TRUE,
    SYS_CmdStream, Open(sh->shell_startup, MODE_OLDFILE),
    SYS_UserShell, TRUE,
    SYS_Input, sh->fh_in,
    SYS_Output, sh->fh_out,
    NP_ExitCode, exit_func,
    NP_ExitData, sh,
    NP_WindowPtr, -1, // disable reqs from DOS
    TAG_DONE);

  // if the shell could not launch then report the exit status from here
  if(result == -1) {
    sh->exit_status = SHELL_EXIT_STATUS_NO_SHELL;
    Signal(sh->call_proc, 1 << sh->exit_signal);
  }

  CloseLibrary((struct Library *)DOSBase);

  return 0;
}
#undef DOSBase

shell_handle_t *shell_init(BPTR fh_in, BPTR fh_out, const char *shell_startup,
  ULONG stack_size)
{
  // setup handle
  shell_handle_t *sh = (shell_handle_t *)AllocVec(sizeof(shell_handle_t), MEMF_ANY);
  if(sh == NULL) {
    return NULL;
  }

  // default startup
  if(shell_startup == NULL) {
    shell_startup = "S:Shell-Startup";
  }
  sh->shell_startup = shell_startup;

  // store caller
  sh->call_proc = FindTask(NULL);

  // create start_signal
  sh->exit_signal = AllocSignal(-1);
  if(sh->exit_signal == -1) {
    FreeVec(sh);
    return NULL;
  }

  sh->exit_status = SHELL_EXIT_STATUS_INVALID;
  sh->fh_in = fh_in;
  sh->fh_out = fh_out;

  // create a sub process we need to launch our shell
  // Note: unfortunately, we cannot launch an async shell here directly
  // with SystemTags(....) since the call will interact synchronously first
  // with the handler of fh_in/fh_out. So its not possible to run the handler
  // in this process eg. via a virtual con without dead locking...
  CreateNewProcTags(
      NP_Entry, start_proc,
      NP_ExitData, sh, // we pass our handle via exit code
      NP_ConsoleTask, NULL,
      NP_WindowPtr, -1,
      TAG_END);

  return sh;
}

ULONG shell_exit_mask(shell_handle_t *sh)
{
  return 1 << sh->exit_signal;
}

BYTE shell_exit(shell_handle_t *sh)
{
  // blocking wait for end of shell
  if(sh->exit_status == SHELL_EXIT_STATUS_INVALID) {
    ULONG mask = shell_exit_mask(sh);
    Wait(mask);
  }

  BYTE result = sh->exit_status;

  FreeSignal(sh->exit_signal);
  FreeVec(sh);

  // return shell result
  return result;
}

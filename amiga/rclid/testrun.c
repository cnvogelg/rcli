#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <clib/alib_protos.h>

#include "run.h"

#define ZERO 0

/* lib bases */
extern struct ExecBase *SysBase;
struct DosLibrary *DOSBase;

void testrun_command(void)
{
  run_handle_t *sh;

  BPTR fh_in = Open("*", MODE_OLDFILE);
  BPTR fh_out = Open("*", MODE_OLDFILE);

  PutStr("Setup command\n");
  sh = run_init(RUN_MODE_COMMAND, fh_in, fh_out, NULL, 8192);
  if(sh == NULL) {
    PutStr("ERROR setting up command...\n");
    return;
  }

  PutStr("Waiting for Command...\n");
  ULONG mask = run_exit_mask(sh);
  Wait(mask);

  PutStr("Exit command\n");
  LONG rc = run_exit(sh);
  Printf("done status=%ld\n", rc);
}

int testrun_shell(void)
{
  run_handle_t *sh;

  BPTR fh_in = Open("*", MODE_OLDFILE);
  BPTR fh_out = Open("*", MODE_OLDFILE);

  PutStr("Setup shell\n");
  sh = run_init(RUN_MODE_SHELL, fh_in, fh_out, NULL, 8192);
  if(sh == NULL) {
    PutStr("ERROR setting up shell...\n");
    return RETURN_ERROR;
  }

  PutStr("Waiting for Shell...\n");
  ULONG mask = run_exit_mask(sh);
  Wait(mask);

  PutStr("Exit shell\n");
  LONG rc = run_exit(sh);
  Printf("done status=%ld\n", rc);

  return RETURN_OK;
}

int main(void)
{
  int result;

  DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR) "dos.library", 0L);

  testrun_command();

  result = testrun_shell();

  CloseLibrary((struct Library *)DOSBase);

  return result;
}

#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <clib/alib_protos.h>

#include "shell.h"

#define ZERO 0

/* lib bases */
extern struct ExecBase *SysBase;
struct DosLibrary *DOSBase;

int testshell(void)
{
  shell_handle_t *sh;

  BPTR fh = Open("*", MODE_OLDFILE);

  PutStr("Setup shell\n");
  sh = shell_init(fh, ZERO, NULL, 8192);
  if(sh == NULL) {
    PutStr("ERROR setting up shell...\n");
    return RETURN_ERROR;
  }

  PutStr("Waiting for Shell...\n");
  ULONG mask = shell_exit_mask(sh);
  Wait(mask);

  PutStr("Exit shell\n");
  LONG rc = shell_exit(sh);
  Printf("done status=%ld\n", rc);

  return RETURN_OK;
}

int main(void)
{
  int result;

  DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR) "dos.library", 0L);

  result = testshell();

  CloseLibrary((struct Library *)DOSBase);

  return result;
}

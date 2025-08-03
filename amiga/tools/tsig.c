#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <clib/alib_protos.h>

struct DosLibrary *DOSBase;

#define DELAY 200000UL

int main(void)
{
  DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR) "dos.library", 0L);

  PutStr("Press CTRL-C/D/E/F...\n");

  ULONG mask = SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F;
  ULONG res = 0;
  for(int i=0;i<5;i++) {
    res = CheckSignal(mask);
    if(res != 0) {
      break;
    }
    Delay(50);
  }

  int exit_code = 0;
  if(res & SIGBREAKF_CTRL_C) {
    PutStr("Ctrl-C\n");
    exit_code += 1;
  }
  if(res & SIGBREAKF_CTRL_D) {
    PutStr("Ctrl-D\n");
    exit_code += 2;
  }
  if(res & SIGBREAKF_CTRL_E) {
    PutStr("Ctrl-E\n");
    exit_code += 4;
  }
  if(res & SIGBREAKF_CTRL_F) {
    PutStr("Ctrl-F\n");
    exit_code += 8;
  }
  if(res == 0) {
    PutStr("No Ctrl key pressed!\n");
  }

  CloseLibrary((struct Library *)DOSBase);

  return exit_code;
}

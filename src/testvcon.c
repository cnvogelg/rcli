#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <clib/alib_protos.h>

#include <string.h>

#include "shell.h"
#include "vcon.h"

#define ZERO 0

/* lib bases */
extern struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
int num_read = 0;

static LONG read_func(APTR data, LONG size, APTR user_data)
{
  LONG ret_size = 0;

  if(num_read == 0) {
    strcpy(data, "list\n");
    ret_size = 5;
  }
  if(num_read == 1) {
    strcpy(data, "endcli\n");
    ret_size = 7;
  }

  num_read++;
  return ret_size;
}

static LONG write_func(APTR data, LONG size, APTR user_data)
{
  PutStr("Data: '");
  Write(Output(), data, size);
  PutStr("'\n");
  return size;
}

int testvcon(void)
{
  shell_handle_t *sh;
  vcon_handle_t *sc;

  PutStr("Setup shell console\n");
  sc = vcon_init();
  if(sc == NULL) {
    PutStr("ERROR setting up shell console...\n");
    return RETURN_ERROR;
  }

  PutStr("Setup shell\n");
  BPTR fh = vcon_get_fh(sc);
  sh = shell_init(fh, ZERO, NULL, 8192);
  if(sh == NULL) {
    PutStr("ERROR setting up shell...\n");
    vcon_exit(sc);
    return RETURN_ERROR;
  }

  PutStr("Main loop...\n");
  ULONG shell_mask = shell_exit_mask(sh);
  ULONG con_mask = vcon_get_sigmask(sc);
  ULONG masks = shell_mask | con_mask | SIGBREAKF_CTRL_C;

  while(TRUE) {
    ULONG got_mask = Wait(masks);
    if(got_mask & con_mask) {
      PutStr("handle console\n");
      ULONG status = vcon_handle(sc);
      Printf("-> status=%lx\n", status);
      if(status & VCON_HANDLE_READ) {
        LONG size = vcon_read_requested(sc);
        Printf("READ size=%ld\n", size);
        vcon_read(sc, read_func, NULL);
        PutStr("READ done.\n");
      }
      if(status & VCON_HANDLE_WRITE) {
        LONG size = vcon_write_pending(sc);
        Printf("WRITE size=%ld\n", size);
        vcon_write(sc, write_func, NULL);
        PutStr("WRITE done.\n");
      }
    }
    if(got_mask & shell_mask) {
      break;
    }
    if(got_mask & SIGBREAKF_CTRL_C) {
      PutStr("*BREAK!\n");
      vcon_drop(sc);
      PutStr("dropped con!\n");
    }
  }

  PutStr("Exit shell\n");
  LONG rc = shell_exit(sh);
  Printf("done rc=%ld\n", rc);
  vcon_exit(sc);
  PutStr("done.\n");

  return RETURN_OK;
}

int main(void)
{
  int result;

  DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR) "dos.library", 0L);

  result = testvcon();

  CloseLibrary((struct Library *)DOSBase);

  return result;
}

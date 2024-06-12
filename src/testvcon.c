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

int testvcon(void)
{
  shell_handle_t *sh;
  vcon_handle_t *sc;
  vcon_buf_t buffer;

  PutStr("Setup shell console\n");
  sc = vcon_init();
  if(sc == NULL) {
    PutStr("ERROR setting up shell console...\n");
    return RETURN_ERROR;
  }

  PutStr("Setup shell\n");
  BPTR fh = vcon_create_fh(sc);
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
      ULONG status = vcon_handle_sigmask(sc, got_mask & con_mask);
      Printf("-> status=%lx\n", status);
      if(status & VCON_HANDLE_READ) {
        LONG size = vcon_read_begin(sc, &buffer);
        Printf("VREAD size=%ld\n", buffer.size);
        //read_func(&buffer);
        buffer.size = Read(Input(), buffer.buffer, buffer.size);
        vcon_read_end(sc, &buffer);
        Printf("VREAD done. size=%ld\n", buffer.size);
      }
      if(status & VCON_HANDLE_WRITE) {
        APTR buf = NULL;
        LONG size = vcon_write_begin(sc, &buffer);;
        Printf("VWRITE size=%ld\n", buffer.size);
        PutStr("VDATA[");
        FWrite(Output(), buffer.buffer, buffer.size, 1);
        PutStr("]\n");
        vcon_write_end(sc, &buffer);
        PutStr("VWRITE done.\n");
      }
      if(status & VCON_HANDLE_WAIT_CHAR) {
        ULONG wait_s = 0, wait_us = 0;
        vcon_waitchar_get_wait_time(sc, &wait_s, &wait_us);
        ULONG total = wait_s * 1000000UL + wait_us;
        Printf("VWAITCHAR: s=%ld us=%ld -> %ld", wait_s, wait_us, total);
        BOOL ok = WaitForChar(Input(), total);
        if(ok) {
          PutStr("REPORT!\n");
          vcon_waitchar_report(sc);
        }
      }
      if(status & VCON_HANDLE_CLOSE) {
        PutStr("VCLOSED console\n");
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

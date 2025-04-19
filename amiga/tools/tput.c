/* a tput-like tool to emit console control codes by name */

#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <clib/alib_protos.h>

#include <string.h>

#include "termio.h"

/* lib bases */
extern struct ExecBase *SysBase;
struct DosLibrary *DOSBase;

/* params */
static const char *TEMPLATE =
  "LIST/S,"
  "VERBOSE/S,"
  "CODE/M";
typedef struct
{
  ULONG list;
  ULONG verbose;
  UBYTE **code;
} params_t;
static params_t params;

#define MAX_BUF 128
static UBYTE expand_buf[MAX_BUF];

static int tput(const UBYTE **cmd, ULONG verbose)
{
  if((cmd == NULL) || (cmd[0] == NULL)) {
    PutStr("No command given! See LIST for all commands.\n");
    return RETURN_WARN;
  }

  /* find code */
  struct termio_cmd_code *code = termio_find_cmd_code(cmd[0]);
  if(code == NULL) {
    Printf("Code '%s' not found!\n", cmd[0]);
    return RETURN_ERROR;
  }

  /* has params? */
  const UBYTE *params = code->params;
  const UBYTE **args = cmd+1;

  /* does the code has params ?*/
  const UBYTE *output;
  LONG len;
  if((params != NULL) && (args[0] != NULL)) {
    /* expand code string with args */
    output = expand_buf;
    len = termio_expand_cmd_code(code, args, expand_buf, MAX_BUF);
    if(len < 0) {
      PutStr("Invalid parameters given...\n");
      return RETURN_ERROR;
    }
    expand_buf[len] = 0;
  } else {
    /* output verbatim */
    output = code->seq;
    len = strlen(output);
  }

  /* output seq */
  if(verbose) {
    termio_dump_buf(output, len);
  } else {
    /* emit code */
    PutStr(output);
    Flush(Output());
  }

  return RETURN_OK;
}

int main(void)
{
  struct RDArgs *args;
  int result = RETURN_OK;
  ULONG wait_delay;

  DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR) "dos.library", 0L);

  /* First parse args */
  args = ReadArgs(TEMPLATE, (LONG *)&params, NULL);
  if (args == NULL)
  {
    PutStr(TEMPLATE);
    PutStr("  Invalid Args!\n");
    return RETURN_ERROR;
  }

  if(params.list) {
    termio_list_cmd_codes();
  } else {
    result = tput(params.code, params.verbose);
  }

  /* free args */
  FreeArgs(args);

  CloseLibrary((struct Library *)DOSBase);

  return result;
}

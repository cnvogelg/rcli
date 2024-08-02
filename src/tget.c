/* a tget tool to parse console control codes */

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
    "VERBOSE/S,"
    "CODE/M";
typedef struct
{
  LONG    verbose;
  UBYTE **code;
} params_t;
static params_t params;

#define DELAY 200000UL

static int read_buf(UBYTE *buf, int max_size)
{
  int size = 0;
  while(size < (max_size - 1)) {
    LONG got = Read(Input(), buf, 1);
    if(got < 1) {
      break;
    }
    buf++;
    size++;

    if(WaitForChar(Input(), DELAY) == FALSE) {
      break;
    }
  }
  buf[size] = 0;
  return size;
}

static int tget(const UBYTE **cmd, LONG verbose)
{
  UBYTE buffer[32];

  SetMode(Input(), 1); // RAW

  LONG len = read_buf(buffer, 32);

  if(verbose) {
    termio_dump_buf(buffer, len);
  }

  struct termio_key_code *code = termio_find_key_code(buffer, len);
  if(code != NULL) {
    Printf("Special Key: '%s'\n", code->name);
  } else {
    struct termio_cmd_seq seq;
    int res = termio_parse_csi(buffer, len, &seq);
    if(res > 0) {
      Printf("CSI Seq: '%lc' (%ld bytes) args: ",
        (LONG)seq.cmd, (LONG)seq.len_bytes);
      for(UBYTE i=0;i<seq.num_args;i++) {
        Printf("%ld ", (LONG)seq.args[i]);
      }
      PutStr("\n");
    } else if(res == TERMIO_ERR_NO_CSI) {
      Printf("Normal Key: '%lc'\n", buffer[0]);
    } else {
      Printf("CSI Parse Error: %ld\n", res);
    }
  }

  return RETURN_OK;
}

int main(void)
{
  struct RDArgs *args;
  int result;
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

  result = tget(params.code, params.verbose);

  /* free args */
  FreeArgs(args);

  CloseLibrary((struct Library *)DOSBase);

  return result;
}

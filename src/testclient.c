#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <clib/alib_protos.h>

/* lib bases */
extern struct ExecBase *SysBase;
struct DosLibrary *DOSBase;

#define TEST_SIGNAL     1
#define TEST_WAITCHAR   2
#define TEST_RAWKEY     4

/* params */
static const char *TEMPLATE =
    "SIGNAL/S,"
    "WAITCHAR/S,"
    "RAWKEY/S,"
    "WAITDELAY/N/K";
typedef struct
{
  ULONG signal;
  ULONG waitchar;
  ULONG rawkey;
  ULONG *wait_delay;
} params_t;
static params_t params;

static void test_signal(void)
{
  PutStr("Press Ctrl+C/D/E/F to exit.\n");

  ULONG wait_mask = SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F;
  ULONG result = Wait(wait_mask);

  PutStr("You pressed ");
  if(result & SIGBREAKF_CTRL_C) {
    PutStr("Ctrl-C");
  }
  if(result & SIGBREAKF_CTRL_D) {
    PutStr("Ctrl-D");
  }
  if(result & SIGBREAKF_CTRL_E) {
    PutStr("Ctrl-E");
  }
  if(result & SIGBREAKF_CTRL_F) {
    PutStr("Ctrl-F");
  }
  PutStr("\n");
}

static void test_waitchar(ULONG wait_delay)
{
  BPTR fh = Input();

  BOOL is = IsInteractive(fh);
  if(!is) {
    PutStr("Input is not interactive!\n");
    return;
  }

  SetMode(fh, 1); // raw mode

  Printf("Waiting for key in %ld us...\n", wait_delay);
  BOOL ok = WaitForChar(Input(), wait_delay);
  if(ok) {
    PutStr("OK!\n");
  } else {
    PutStr("timeout.\n");
  }

  SetMode(fh, 0); // cooked mode
}

static void test_rawkey(void)
{
  BPTR fh = Input();

  SetMode(fh, 1); // raw mode

  PutStr("Enter key...\n");
  UBYTE key[8];
  int n = Read(fh, &key, 7);
  for(int i=0;i<n;i++) {
    Printf("got: %ld@0x%lx\n", i, (ULONG)key[i]);
  }

  SetMode(fh, 0); // cooked mode

  // show any signal present
  ULONG wait_mask = SIGBREAKF_CTRL_C | SIGBREAKF_CTRL_D | SIGBREAKF_CTRL_E | SIGBREAKF_CTRL_F;
  ULONG result = CheckSignal(wait_mask);

  PutStr("You pressed ");
  if(result & SIGBREAKF_CTRL_C) {
    PutStr("Ctrl-C");
  }
  if(result & SIGBREAKF_CTRL_D) {
    PutStr("Ctrl-D");
  }
  if(result & SIGBREAKF_CTRL_E) {
    PutStr("Ctrl-E");
  }
  if(result & SIGBREAKF_CTRL_F) {
    PutStr("Ctrl-F");
  }
  PutStr("\n");
}

int testclient(int test_modes, ULONG wait_delay)
{
  if(test_modes & TEST_SIGNAL) {
    test_signal();
  }
  if(test_modes & TEST_WAITCHAR) {
    test_waitchar(wait_delay);
  }
  if(test_modes & TEST_RAWKEY) {
    test_rawkey();
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

  if(params.wait_delay != NULL) {
    wait_delay = *params.wait_delay * 1000UL; // ms
  } else {
    wait_delay = 5000000UL;
  }

  int test_modes = 0;
  if(params.signal) {
    test_modes |= TEST_SIGNAL;
  }
  if(params.waitchar) {
    test_modes |= TEST_WAITCHAR;
  }
  if(params.rawkey) {
    test_modes |= TEST_RAWKEY;
  }

  result = testclient(test_modes, wait_delay);

  /* free args */
  FreeArgs(args);

  CloseLibrary((struct Library *)DOSBase);

  return result;
}

#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>

#include <sys/ioctl.h>

#include "serv.h"

/* lib bases */
extern struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct Library *SocketBase;

/* params */
static const char *TEMPLATE =
    "SOCKET/N/K,"
    "TASK/N/K,"
    "SIGNAL/N/K";
typedef struct
{
  ULONG *socket;
  ULONG *task;
  ULONG *signal;
} params_t;
static params_t params;

int rclid(void)
{
  int socket;

  // launched by runserv?
  if((params.socket != NULL) && (params.task != NULL) && (params.signal != NULL)) {
    PutStr("runserv launched me...\n");

    // retrieve socket
    ULONG sock_id = *params.socket;
    Printf("obtaining socket %lu\n", sock_id);
    socket = ObtainSocket(sock_id, AF_INET, SOCK_STREAM, 0);

    // signal runserv we took over
    struct Task *runserv_task = (struct Task *)*params.task;
    ULONG mask = 1 << *params.signal;
    Printf("sending signal %ld to task %lx\n", (ULONG)*params.signal, (ULONG)runserv_task);
    Signal(runserv_task, mask);

  } else {
    PutStr("inetd launched me...\n");

    // get daemon msg set by inetd for us
    struct Process *me = (struct Process *)FindTask(NULL);
    struct DaemonMessage *dm = (struct DaemonMessage *)me->pr_ExitData;
    if(dm == NULL) {
      Printf("failed getting daemon msg!\n");
      return RETURN_FAIL;
    }

    socket = ObtainSocket(dm->dm_ID, dm->dm_Family, dm->dm_Type, 0);
  }

  // check socket we took over
  if(socket == -1) {
    PutStr("Failed to obtain socket!\n");
    return RETURN_FAIL;
  }

  PutStr("rclid service ready.\n");

  // make socket non blocking
  ULONG yes=TRUE;
  IoctlSocket(socket, FIONBIO, (char *)&yes);

  serv_main(socket);

  CloseSocket(socket);
  PutStr("rclid service done.\n");
  return RETURN_OK;
}

int main(void)
{
  struct RDArgs *args;
  int result;

  DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR) "dos.library", 0L);
  if(DOSBase == NULL) {
    PutStr("Error opening DOS lib!\n");
    return RETURN_FAIL;
  }

  SocketBase = OpenLibrary((STRPTR)"bsdsocket.library", 0L);
  if(SocketBase == NULL) {
    PutStr("Error opening Socket lib!\n");
    CloseLibrary((struct Library *)DOSBase);
    return RETURN_FAIL;
  }

  /* First parse args */
  args = ReadArgs(TEMPLATE, (LONG *)&params, NULL);
  if (args == NULL)
  {
    PutStr(TEMPLATE);
    PutStr("  Invalid Args!\n");
    return RETURN_ERROR;
  }

  result = rclid();

  /* free args */
  FreeArgs(args);

  CloseLibrary(SocketBase);
  CloseLibrary((struct Library *)DOSBase);

  return result;
}

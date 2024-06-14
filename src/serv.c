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

static int get_socket(void)
{
  int socket;

  // launched by runserv?
  if((params.socket != NULL) && (params.task != NULL) && (params.signal != NULL)) {
    PutStr("serv: runserv launched me...\n");

    // retrieve socket
    ULONG sock_id = *params.socket;
    Printf("serv: obtaining socket %lu\n", sock_id);
    socket = ObtainSocket(sock_id, AF_INET, SOCK_STREAM, 0);

    // signal runserv we took over
    struct Task *runserv_task = (struct Task *)*params.task;
    ULONG mask = 1 << *params.signal;
    Printf("serv: sending signal %ld to runserv task %lx\n", (ULONG)*params.signal, (ULONG)runserv_task);
    Signal(runserv_task, mask);

  } else {
    PutStr("serv: inetd launched me...\n");

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
    return -1;
  }

  // make socket non blocking
  ULONG yes=TRUE;
  IoctlSocket(socket, FIONBIO, (char *)&yes);

  return socket;
}

int serv_init(void)
{
  struct RDArgs *args;
  int result;

  DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR) "dos.library", 0L);
  if(DOSBase == NULL) {
    PutStr("Error opening DOS lib!\n");
    return -1;
  }

  SocketBase = OpenLibrary((STRPTR)"bsdsocket.library", 0L);
  if(SocketBase == NULL) {
    PutStr("Error opening Socket lib!\n");
    CloseLibrary((struct Library *)DOSBase);
    return -1;
  }

  /* First parse args */
  args = ReadArgs(TEMPLATE, (LONG *)&params, NULL);
  if (args == NULL)
  {
    PutStr(TEMPLATE);
    PutStr("  Invalid Args!\n");
    return -1;
  }

  int socket = get_socket();

  /* free args */
  FreeArgs(args);

  Printf("serv: ready (socket %ld)\n", socket);

  return socket;
}

void serv_exit(int socket)
{
  CloseSocket(socket);
  CloseLibrary(SocketBase);
  CloseLibrary((struct Library *)DOSBase);

  PutStr("serv: done.\n");
}

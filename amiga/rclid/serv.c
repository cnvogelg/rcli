#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>

#include "serv.h"

/* lib bases */
extern struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct Library *SocketBase;

/* params */
static struct RDArgs *args;
static const char *serv_name;

static int get_socket(serv_params_t *params)
{
  int socket;

  // launched by runserv?
  if((params->socket != NULL) && (params->task != NULL) && (params->signal != NULL)) {
    // retrieve socket
    ULONG sock_id = *params->socket;
    socket = ObtainSocket(sock_id, AF_INET, SOCK_STREAM, 0);
    Printf("%s: obtaining runserv socket id %lu -> %lu\n", serv_name, sock_id, socket);

    // signal runserv we took over
    struct Task *runserv_task = (struct Task *)*params->task;
    ULONG signal = *params->signal;
    ULONG mask = 1 << signal;
    Printf("%s: sending signal %ld to runserv task %lx\n", serv_name, signal, (ULONG)runserv_task);
    Signal(runserv_task, mask);

  } else {
    // get daemon msg set by inetd for us
    struct Process *me = (struct Process *)FindTask(NULL);
    struct DaemonMessage *dm = (struct DaemonMessage *)me->pr_ExitData;
    if(dm == NULL) {
      Printf("%s: failed getting inetd daemon msg!\n", serv_name);
      return -1;
    }

    socket = ObtainSocket(dm->dm_ID, dm->dm_Family, dm->dm_Type, 0);
    Printf("%s: obtaining runserv socket id %lu -> %lu\n", serv_name, dm->dm_ID, socket);
  }

  // check socket we took over
  if(socket == -1) {
    Printf("%s: Failed to obtain socket!\n", serv_name);
  }

  return socket;
}

int serv_init(const char *name, const char *template, serv_params_t *params)
{
  int result;

  serv_name = name;

  DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR) "dos.library", 0L);
  if(DOSBase == NULL) {
    PutStr(name);
    PutStr(": Error opening DOS lib!\n");
    return -1;
  }

  SocketBase = OpenLibrary((STRPTR)"bsdsocket.library", 0L);
  if(SocketBase == NULL) {
    PutStr(name);
    PutStr(": Error opening Socket lib!\n");
    CloseLibrary((struct Library *)DOSBase);
    return -1;
  }

  /* First parse args */
  args = ReadArgs(template, (LONG *)params, NULL);
  if (args == NULL)
  {
    Printf("%s: inavlid args! %s\n", name, template);
    return -1;
  }

  /* retrieve socket from args */
  int socket = get_socket(params);
  if(socket == -1) {
    FreeArgs(args);
    return -1;
  }

  Printf("%s: ready (socket %ld)\n", name, socket);

  return socket;
}

void serv_exit(int socket)
{
  CloseSocket(socket);
  CloseLibrary(SocketBase);
  CloseLibrary((struct Library *)DOSBase);

  /* free args */
  FreeArgs(args);

  Printf("%s: done.\n", serv_name);
}

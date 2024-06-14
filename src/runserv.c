#include <exec/exec.h>
#include <dos/dos.h>
#include <dos/dostags.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>

#include <string.h>
#include <stdio.h>

#include "compiler.h"

/* lib bases */
extern struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
struct Library *SocketBase;

/* params */
static const char *TEMPLATE =
    "-P=PORT/N/K,"
    "-S=SERVICE/K,"
    "STACK/N/K,"
    "-V=VERBOSE/S";
typedef struct
{
  ULONG *port;
  char  *service;
  ULONG *stack;
  ULONG verbose;
} params_t;
static params_t params;

LONG setup_listen_socket(ULONG port)
{
  struct sockaddr_in listen_addr;

  // create socket
  LONG listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(listen_fd < 0) {
    PutStr("Failed opening socket!\n");
    return -1;
  }

  // bind socket
  memset(&listen_addr, 0, sizeof(struct sockaddr_in));
  listen_addr.sin_port = port;
  listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_len = sizeof(listen_addr);
  LONG res = bind(listen_fd, (struct sockaddr *)&listen_addr, sizeof(listen_addr));
  if(res == -1) {
    PutStr("Can't bind socket!\n");
    CloseSocket(listen_fd);
    return -1;
  }

  // start listening
  res = listen(listen_fd, 5); // backlog size
  if(res == -1) {
    PutStr("Can't listen on socket!\n");
    CloseSocket(listen_fd);
    return -1;
  }

  return listen_fd;
}

/* ----- tool ----- */
int runserv(ULONG port, const char *service, ULONG stack)
{
  // get sync signal
  BYTE sync_signal = AllocSignal(-1);
  if(sync_signal == -1) {
    PutStr("No Signal?\n");
    return RETURN_ERROR;
  }

  // get listen socket
  LONG listen_fd = setup_listen_socket(port);
  if(listen_fd < 0) {
    FreeSignal(sync_signal);
    return RETURN_ERROR;
  }

  // setup fd set
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(listen_fd, &fds);

  // main loop
  BOOL stay = TRUE;
  while(stay) {

    // open socket on any addr on given port
    Printf("runserv: waiting for connection on port %lu. Use Ctrl-C to abort.\n", port);

    // wait/select for accept event or break
    LONG n = WaitSelect(listen_fd + 1, &fds, NULL, NULL, NULL, 0);
    if(n == -1) {
      if(Errno() == EINTR) {
        PutStr("*Break\n");
      } else {
        PutStr("Failed select()\n");
      }
      break;
    }

    // accept connection
    struct sockaddr_in client_addr;
    LONG addr_len = sizeof(struct sockaddr_in);
    LONG client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
    if(client_fd < 0) {
      PutStr("Failed accepting client socket!\n");
      break;
    }
    if(addr_len != sizeof(struct sockaddr_in)) {
      PutStr("Invalid addr size!");
      break;
    }
    Printf("runserv: client from %08lx:%ld connected\n",
        client_addr.sin_addr, client_addr.sin_port);

    // launch service
    BPTR srv_seg = LoadSeg(service);
    if(srv_seg == 0) {
      CloseSocket(client_fd);
      Printf("Service '%s' not found!\n", service);
      break;
    }

    // release socket to pass it on to service
    LONG sock_id = ReleaseSocket(client_fd, UNIQUE_ID);
    if(sock_id == -1) {
      CloseSocket(client_fd);
      Printf("Can't release socket!\n");
      break;
    }

    // prepare args for service
    char arg_line[80];
    struct Task *my_task = FindTask(NULL);
    sprintf(arg_line, "SOCKET %lu TASK %lu SIGNAL %lu",
        (ULONG)sock_id, (ULONG)my_task, (ULONG)sync_signal);
    Printf("runserv: launching '%s' with args '%s' and stack %ld\n",
        service, arg_line, stack);

    // clear sync signal
    ULONG sync_mask = 1 << sync_signal;
    SetSignal(0, sync_mask);

    // clone output
    BPTR out = Open("*", MODE_OLDFILE);

    // launch process
    struct Process *srv_proc = CreateNewProcTags(
      NP_Seglist, srv_seg,
      NP_Name, service,
      NP_Arguments, arg_line,
      NP_StackSize, stack,
      NP_FreeSeglist, TRUE,
      NP_Output, out,
      TAG_END);

    // wait for sync from service
    PutStr("runserv: waiting for service to launch. Ctrl-C to break...\n");
    ULONG wait_mask = sync_mask | SIGBREAKF_CTRL_C;
    ULONG got_mask = Wait(wait_mask);
    if(got_mask & sync_mask) {
      PutStr("runserv: service launched!\n");
    }
    else {
      // get socket back to clean up
      client_fd = ObtainSocket(sock_id, AF_INET, SOCK_STREAM, 0);
      CloseSocket(client_fd);
    }

    // aborted...
    if(got_mask & SIGBREAKF_CTRL_C) {
      PutStr("*Break\n");
      break;
    }
  }

  PutStr("runserv: shuttting down.\n");
  CloseSocket(listen_fd);

  FreeSignal(sync_signal);

  return RETURN_OK;
}

/* ---------- main ---------- */
int main(void)
{
  struct RDArgs *args;
  int result;
  ULONG port;
  const char *service;
  ULONG stack;

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

  if(params.port != NULL) {
    port = *params.port;
  } else {
    port = 23;
  }

  if(params.service != NULL) {
    service = params.service;
  } else {
    service = "rclid";
  }

  if(params.stack != NULL) {
    stack = *params.stack;
  } else {
    stack = 12000;
  }

  result = runserv(port, service, stack);

  /* free args */
  FreeArgs(args);

  CloseLibrary(SocketBase);
  CloseLibrary((struct Library *)DOSBase);

  return result;
}

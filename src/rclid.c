#include <exec/exec.h>
#include <dos/dos.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/socket.h>
#include <clib/alib_protos.h>

#include "serv.h"

extern struct DosLibrary *DOSBase;
extern struct Library *SocketBase;

void serv_main(int socket)
{
  send(socket, "huhu!\n", 6, 0);
}

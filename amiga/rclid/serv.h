#ifndef SERV_H
#define SERV_H

#define SERV_ARGS_TEMPLATE  "SOCKET/N/K,TASK/N/K,SIGNAL/N/K"

typedef struct
{
  ULONG *socket;
  ULONG *task;
  ULONG *signal;
} serv_params_t;

/* return server socket or -1 on failure */
extern int serv_init(const char *name, const char *template, serv_params_t *param);

/* cleanup server */
extern void serv_exit(int socket);

#endif

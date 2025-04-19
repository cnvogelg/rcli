#ifndef SERV_H
#define SERV_H

/* return server socket or -1 on failure */
extern int serv_init(void);

/* cleanup server */
extern void serv_exit(int socket);

#endif

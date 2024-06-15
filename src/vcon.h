#ifndef VCON
#define VCON

#include "buf.h"

/* flags returned by vcon_handle() */
#define VCON_HANDLE_READ       1 /* con likes to read something next */
#define VCON_HANDLE_WRITE      2 /* con likes to write something next */
#define VCON_HANDLE_CLOSE      4 /* con was closed */
#define VCON_HANDLE_MODE       8 /* con mode was changed */
#define VCON_HANDLE_WAIT_BEGIN 16 /* first is waiting for char */
#define VCON_HANDLE_WAIT_END   32 /* no one waiting for char anymore */

struct vcon_handle;
typedef struct vcon_handle vcon_handle_t;

vcon_handle_t *vcon_init(void);
void vcon_exit(vcon_handle_t *sh);

/* create a new file handle for vcon. has to be closed on your own! */
BPTR  vcon_create_fh(vcon_handle_t *sh);

/* after VCON_HANDLE_MODE retrieve now mode value */
LONG  vcon_get_buffer_mode(vcon_handle_t *sh);

/* return the sigmask the handle call will respond to */
ULONG vcon_get_sigmask(vcon_handle_t *sh);

/* send a signal mask to the console SIGBREAKF_CTRL_C ... */
BOOL  vcon_send_signal(vcon_handle_t *sh, ULONG sig_mask);

/* process the signals the vcon handles. return VCON_HANDLE_xx */
ULONG vcon_handle_sigmask(vcon_handle_t *sh, ULONG sig_mask);

/* something to read. buf must be empty and is set to external data/size/capacity */
BOOL vcon_read_begin(vcon_handle_t *sh, buf_t *buf);

/* done reading. set the total number of bytes read before in buf->size */
void vcon_read_end(vcon_handle_t *sh, buf_t *buf);

/* is something to write pending and how much. buf must be emtpy and is set to external data/size/capacity */
BOOL vcon_write_begin(vcon_handle_t *sh, buf_t *buf);

/* done writing. set the toal number of bytes written */
void vcon_write_end(vcon_handle_t *sh, buf_t *buf);

/* get waiting time of first one waiting */
BOOL vcon_wait_char_get_wait_time(vcon_handle_t *sh, ULONG *wait_s, ULONG *wait_us);

/* report a waiting key */
BOOL vcon_wait_char_report(vcon_handle_t *sh);

/* drop all pending io reqs */
void vcon_drop(vcon_handle_t *sh);

#endif
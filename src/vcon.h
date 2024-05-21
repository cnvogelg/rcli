#ifndef VCON
#define VCON

/* flags returned by vcon_handle() */
#define VCON_HANDLE_READ      1 /* con likes to read something next */
#define VCON_HANDLE_WRITE     2 /* con likes to write something next */
#define VCON_HANDLE_CLOSE     4 /* con was closed */
#define VCON_HANDLE_MODE      8 /* con mode was changed */

struct vcon_handle;
typedef struct vcon_handle vcon_handle_t;

typedef LONG (*vcon_io_func_t)(APTR data, LONG size, APTR user_data);

vcon_handle_t *vcon_init(void);
void vcon_exit(vcon_handle_t *sh);

BPTR  vcon_get_fh(vcon_handle_t *sh);
LONG  vcon_get_buffer_mode(vcon_handle_t *sh);
ULONG vcon_get_sigmask(vcon_handle_t *sh);

/* send a signal mask to the console SIGBREAKF_CTRL_C ... */
BOOL  vcon_signal(vcon_handle_t *sh, ULONG sig_mask);

/* process the signals the vcon handles. return VCON_HANDLE_xx */
ULONG vcon_handle(vcon_handle_t *sh);

/* is something to read pending and how much? */
LONG vcon_read_requested(vcon_handle_t *sh);

/* let the shell read from my buffer.
   return number of bytes actually consumed
   return -1 if read not possible, e.g. no read requested
 */
LONG vcon_read(vcon_handle_t *sh, vcon_io_func_t read_func, APTR user_data);

/* is something to write pending and how much? */
LONG vcon_write_pending(vcon_handle_t *sh);

/* let the shell write.
   return number of bytes actually consumed
   return -1 if write not possible, e.g. no write pending
 */
LONG vcon_write(vcon_handle_t *sh, vcon_io_func_t write_func, APTR user_data);

/* drop all pending io reqs */
void vcon_drop(vcon_handle_t *sh);

#endif
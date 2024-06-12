#ifndef VCON
#define VCON

/* flags returned by vcon_handle() */
#define VCON_HANDLE_READ      1 /* con likes to read something next */
#define VCON_HANDLE_WRITE     2 /* con likes to write something next */
#define VCON_HANDLE_CLOSE     4 /* con was closed */
#define VCON_HANDLE_MODE      8 /* con mode was changed */
#define VCON_HANDLE_WAIT_CHAR 16 /* someone is waiting for a key */

struct vcon_handle;
typedef struct vcon_handle vcon_handle_t;

struct vcon_buf {
  UBYTE *data;
  LONG  size;
  APTR  private;
};
typedef struct vcon_buf vcon_buf_t;

vcon_handle_t *vcon_init(void);
void vcon_exit(vcon_handle_t *sh);

/* create a new file handle for vcon. has to be closed on your own! */
BPTR  vcon_create_fh(vcon_handle_t *sh);

LONG  vcon_get_buffer_mode(vcon_handle_t *sh);
ULONG vcon_get_sigmask(vcon_handle_t *sh);

/* send a signal mask to the console SIGBREAKF_CTRL_C ... */
BOOL  vcon_send_signal(vcon_handle_t *sh, ULONG sig_mask);

/* process the signals the vcon handles. return VCON_HANDLE_xx */
ULONG vcon_handle_sigmask(vcon_handle_t *sh, ULONG sig_mask);

/* something to read. return size and buffer */
BOOL vcon_read_begin(vcon_handle_t *sh, vcon_buf_t *buf);

/* done reading. set the total number of bytes read */
void vcon_read_end(vcon_handle_t *sh, vcon_buf_t *buf);

/* is something to write pending and how much? */
BOOL vcon_write_begin(vcon_handle_t *sh, vcon_buf_t *buf);

/* done writing. set the toal number of bytes written */
void vcon_write_end(vcon_handle_t *sh, vcon_buf_t *buf);

/* get waiting time of current waiting key */
BOOL vcon_waitchar_get_wait_time(vcon_handle_t *sh, ULONG *wait_s, ULONG *wait_us);

/* report a waiting key */
BOOL vcon_waitchar_report(vcon_handle_t *sh);

/* drop all pending io reqs */
void vcon_drop(vcon_handle_t *sh);

#endif
#ifndef RUN_H
#define RUN_H

#define RUN_EXIT_STATUS_INVALID   -1
#define RUN_EXIT_STATUS_OK        0
#define RUN_EXIT_STATUS_NO_DOS    1
#define RUN_EXIT_STATUS_NO_RUN    2

#define RUN_MODE_SHELL    0
#define RUN_MODE_COMMAND  1

struct run_handle;
typedef struct run_handle run_handle_t;

/* launch an async shell process and execute shell startup */
run_handle_t *run_init(int mode, BPTR fh_in, BPTR fh_out, const char *cmd,
  ULONG stack_size);

/* wait for this mask to the end of the async shell */
ULONG run_exit_mask(run_handle_t *sh);

/* clean up after the shell exited. do not call before Wait()ing for shell shutdown
   return the RUN_EXIT_STATUS_*
*/
BYTE run_exit(run_handle_t *sh);

#endif

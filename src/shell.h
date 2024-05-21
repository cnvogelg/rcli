#ifndef SHELL_H
#define SHELL_H

#define SHELL_EXIT_STATUS_INVALID   -1
#define SHELL_EXIT_STATUS_OK        0
#define SHELL_EXIT_STATUS_NO_DOS    1
#define SHELL_EXIT_STATUS_NO_SHELL  2

struct shell_handle;
typedef struct shell_handle shell_handle_t;

/* launch an async shell process and execute shell startup */
shell_handle_t *shell_init(BPTR in_fh, BPTR out_fh, const char *shell_startup,
  ULONG stack_size);

/* wait for this mask to the end of the async shell */
ULONG shell_exit_mask(shell_handle_t *sh);

/* clean up after the shell exited. do not call before Wait()ing for shell shutdown
   return the SHELL_EXIT_STATUS_*
*/
BYTE shell_exit(shell_handle_t *sh);

#endif
